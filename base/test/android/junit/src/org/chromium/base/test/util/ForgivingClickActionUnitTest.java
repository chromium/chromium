// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

import androidx.test.espresso.AppNotIdleException;
import androidx.test.espresso.PerformException;
import androidx.test.espresso.UiController;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Function;

/** Unit tests for {@link ForgivingClickAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class ForgivingClickActionUnitTest {
    @Test
    public void testForgivingClick_slowFrameDoesNotTriggerLongPress() {
        View view = createView();
        AtomicBoolean clickFired = new AtomicBoolean(false);
        AtomicBoolean longClickFired = new AtomicBoolean(false);
        view.setOnClickListener(v -> clickFired.set(true));
        view.setOnLongClickListener(
                v -> {
                    longClickFired.set(true);
                    return true;
                });

        List<Integer> actions = new ArrayList<>();
        List<Long> eventTimes = new ArrayList<>();
        UiController uiController =
                createSequenceUiController(
                        events -> {
                            for (MotionEvent event : events) {
                                actions.add(event.getAction());
                                eventTimes.add(event.getEventTime());
                                view.dispatchTouchEvent(event);
                                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                                    ShadowSystemClock.advanceBy(
                                            Duration.ofMillis(
                                                    ViewConfiguration.getLongPressTimeout() + 100));
                                }
                            }
                            return true;
                        });

        ForgivingClickAction.forgivingClick().perform(uiController, view);
        assertEquals(List.of(MotionEvent.ACTION_DOWN, MotionEvent.ACTION_UP), actions);
        assertTrue(eventTimes.get(1) - eventTimes.get(0) < ViewConfiguration.getLongPressTimeout());
        assertTrue("Click listener must be triggered", clickFired.get());
        assertFalse("Long click listener must not be triggered", longClickFired.get());
    }

    @Test
    public void testForgivingClick_appNotIdleForgivenOnlyAfterInjectionCompletes() {
        View view = createView();
        ForgivingClickAction clickAction = ForgivingClickAction.forgivingClick();

        // MAIN_LOOPER_HAS_IDLED without MOTION_INJECTION_HAS_COMPLETED means injection completed.
        UiController idleTimeoutAfterInjection =
                createSequenceUiController(
                        events -> {
                            throw AppNotIdleException.create(
                                    List.of("MAIN_LOOPER_HAS_IDLED"), "Animation running");
                        });
        clickAction.perform(idleTimeoutAfterInjection, view);

        // MOTION_INJECTION_HAS_COMPLETED pending means the tap did not finish injecting.
        UiController incompleteInjection =
                createSequenceUiController(
                        events -> {
                            throw AppNotIdleException.create(
                                    List.of(
                                            "MOTION_INJECTION_HAS_COMPLETED",
                                            "MAIN_LOOPER_HAS_IDLED"),
                                    "Injection did not complete");
                        });
        assertThrows(PerformException.class, () -> clickAction.perform(incompleteInjection, view));

        // MAIN_LOOPER_HAS_IDLED thrown during MotionEvents.sendCancel() after sequence failure
        // must not be forgiven as a successful click.
        UiController cancelIdleTimeout =
                createUiController(
                        events -> false,
                        event -> {
                            throw AppNotIdleException.create(
                                    List.of("MAIN_LOOPER_HAS_IDLED"), "Animation running");
                        });
        assertThrows(PerformException.class, () -> clickAction.perform(cancelIdleTimeout, view));
    }

    private static View createView() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        View view = new View(activity);
        activity.setContentView(view);
        view.layout(0, 0, 100, 100);
        return view;
    }

    private static UiController createSequenceUiController(
            Function<Iterable<MotionEvent>, Boolean> sequenceInjector) {
        return createUiController(sequenceInjector, event -> false);
    }

    private static UiController createUiController(
            Function<Iterable<MotionEvent>, Boolean> sequenceInjector,
            Function<MotionEvent, Boolean> eventInjector) {
        return new UiController() {
            @Override
            public boolean injectMotionEvent(MotionEvent event) {
                return eventInjector.apply(event);
            }

            @Override
            public boolean injectMotionEventSequence(Iterable<MotionEvent> events) {
                return sequenceInjector.apply(events);
            }

            @Override
            public boolean injectKeyEvent(KeyEvent event) {
                return false;
            }

            @Override
            public boolean injectString(String str) {
                return false;
            }

            @Override
            public void loopMainThreadUntilIdle() {}

            @Override
            public void loopMainThreadForAtLeast(long millisDelay) {
                ShadowLooper.idleMainLooper();
            }
        };
    }
}
