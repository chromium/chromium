// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.support.annotation.IntDef;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.action.ViewActions;
import android.support.test.espresso.matcher.ViewMatchers;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.UiDevice;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ntp.NtpUiCaptureTestData;
import org.chromium.chrome.browser.suggestions.SuggestionsSheetVisibilityChangeObserverTest.TestVisibilityChangeObserver.Event;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.suggestions.DummySuggestionsEventReporter;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.ui.test.util.UiRestriction;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for {@link SuggestionsSheetVisibilityChangeObserver}.
 */
@DisabledTest(message = "https://crbug.com/805160")
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class SuggestionsSheetVisibilityChangeObserverTest {
    @Rule
    public BottomSheetTestRule mActivityRule = new BottomSheetTestRule();

    @Rule
    public SuggestionsDependenciesRule createSuggestions() {
        mEventReporter = new SpyEventReporter();
        SuggestionsDependenciesRule.TestFactory depsFactory = NtpUiCaptureTestData.createFactory();
        depsFactory.eventReporter = mEventReporter;
        return new SuggestionsDependenciesRule(depsFactory);
    }

    private SpyEventReporter mEventReporter;
    private TestVisibilityChangeObserver mObserver;

    @Before
    public void setUp() throws InterruptedException {
        mActivityRule.startMainActivityOnBottomSheet(BottomSheet.SheetState.PEEK);
        // The home sheet should not be initialised.
        mEventReporter.surfaceOpenedHelper.verifyCallCount();

        // Register the change observer
        mObserver = new TestVisibilityChangeObserver(mActivityRule.getActivity());
        mObserver.expectEvents();
    }

    @Test
    @MediumTest
    public void testHomeSheetVisibilityOnWebPage() {
        // Pull sheet to half. We use the animated variants to be closer to user triggered events.
        mActivityRule.setSheetState(BottomSheet.SheetState.HALF, true);
        mObserver.expectEvents(Event.INITIAL_REVEAL, Event.STATE_CHANGE);
        mEventReporter.surfaceOpenedHelper.waitForCallback();

        // Pull sheet to full.
        mActivityRule.setSheetState(BottomSheet.SheetState.FULL, true);
        mObserver.expectEvents(Event.STATE_CHANGE);

        // close
        Espresso.pressBack();
        mObserver.expectEvents(Event.HIDDEN, Event.STATE_CHANGE, Event.STATE_CHANGE);
    }

    @Test
    @MediumTest
    public void testHomeSheetVisibilityOnOmnibox() {
        // Tap the omnibox. The home sheet content should not be notified it is selected.
        Espresso.onView(ViewMatchers.withId(R.id.url_bar)).perform(ViewActions.click());
        waitForWindowUpdates();

        mObserver.expectEvents();
        assertEquals(BottomSheet.SheetState.FULL, mActivityRule.getBottomSheet().getSheetState());

        // Back closes the bottom sheet.
        Espresso.pressBack();
        waitForWindowUpdates();

        mObserver.expectEvents();
        assertEquals(BottomSheet.SheetState.PEEK, mActivityRule.getBottomSheet().getSheetState());

        mEventReporter.surfaceOpenedHelper.verifyCallCount();
    }

    @Test
    @MediumTest
    public void testHomeSheetVisibilityOnOmniboxAndSwipeToolbar() {
        // Tap the omnibox. The home sheet content should not be notified it is selected.
        Espresso.onView(ViewMatchers.withId(R.id.url_bar)).perform(ViewActions.click());
        waitForWindowUpdates();

        mObserver.expectEvents();
        assertEquals(BottomSheet.SheetState.FULL, mActivityRule.getBottomSheet().getSheetState());

        // Changing the state of the sheet closes the omnibox suggestions and shows the home sheet.
        mActivityRule.setSheetState(BottomSheet.SheetState.HALF, true);
        mObserver.expectEvents(Event.INITIAL_REVEAL, Event.STATE_CHANGE);
        mEventReporter.surfaceOpenedHelper.waitForCallback();

        // Back closes the bottom sheet.
        Espresso.pressBack();
        mObserver.expectEvents(Event.HIDDEN, Event.STATE_CHANGE, Event.STATE_CHANGE);
        assertEquals(BottomSheet.SheetState.PEEK, mActivityRule.getBottomSheet().getSheetState());

        mEventReporter.surfaceOpenedHelper.verifyCallCount();
    }

    static class TestVisibilityChangeObserver extends SuggestionsSheetVisibilityChangeObserver {
        @IntDef({Event.INITIAL_REVEAL, Event.SHOWN, Event.HIDDEN, Event.STATE_CHANGE})
        @Retention(RetentionPolicy.SOURCE)
        public @interface Event {
            int INITIAL_REVEAL = 0;
            int SHOWN = 1;
            int HIDDEN = 2;
            int STATE_CHANGE = 3;
        }

        private final ChromeActivity mActivity;

        private SelfVerifyingCallbackHelper mInitialRevealHelper =
                new SelfVerifyingCallbackHelper("InitialReveal");
        private SelfVerifyingCallbackHelper mShownHelper = new SelfVerifyingCallbackHelper("Shown");
        private SelfVerifyingCallbackHelper mHiddenHelper =
                new SelfVerifyingCallbackHelper("Hidden");
        private SelfVerifyingCallbackHelper mStateChangedHelper =
                new SelfVerifyingCallbackHelper("StateChanged");

        public TestVisibilityChangeObserver(ChromeActivity chromeActivity) {
            super(null, chromeActivity);
            mActivity = chromeActivity;
        }

        @Override
        public void onContentShown(boolean isFirstShown) {
            if (isFirstShown) {
                mInitialRevealHelper.notifyCalled();
            } else {
                mShownHelper.notifyCalled();
            }
        }

        @Override
        public void onContentHidden() {
            mHiddenHelper.notifyCalled();
        }

        @Override
        public void onContentStateChanged(int contentState) {
            mStateChangedHelper.notifyCalled();
        }

        public void expectEvents(@Event int... events) {
            for (@Event int e : events) {
                switch (e) {
                    case Event.INITIAL_REVEAL:
                        mInitialRevealHelper.waitForCallback();
                        break;
                    case Event.SHOWN:
                        mShownHelper.waitForCallback();
                        break;
                    case Event.HIDDEN:
                        mHiddenHelper.waitForCallback();
                        break;
                    case Event.STATE_CHANGE:
                        mStateChangedHelper.waitForCallback();
                        break;
                }
            }
            verifyCalls();
        }

        public void verifyCalls() {
            mInitialRevealHelper.verifyCallCount();
            mShownHelper.verifyCallCount();
            mHiddenHelper.verifyCallCount();
            mStateChangedHelper.verifyCallCount();
        }

        @Override
        protected boolean isObservedContentCurrent() {
            BottomSheet.BottomSheetContent currentSheet =
                    mActivity.getBottomSheet().getCurrentSheetContent();
            return currentSheet != null;
        }
    }

    private static class SelfVerifyingCallbackHelper extends CallbackHelper {
        private final String mName;
        private int mVerifiedCallCount;

        public SelfVerifyingCallbackHelper(String name) {
            mName = name;
        }

        @Override
        public void waitForCallback() {
            try {
                super.waitForCallback(mName + " not called.", mVerifiedCallCount);
                mVerifiedCallCount += 1;
            } catch (InterruptedException | TimeoutException e) {
                throw new AssertionError(e);
            }
        }

        public void verifyCallCount() {
            assertEquals(mName + " call count", mVerifiedCallCount, getCallCount());
        }
    }

    private static class SpyEventReporter extends DummySuggestionsEventReporter {
        public final SelfVerifyingCallbackHelper surfaceOpenedHelper =
                new SelfVerifyingCallbackHelper("onSurfaceOpened");

        @Override
        public void onSurfaceOpened() {
            surfaceOpenedHelper.notifyCalled();
        }
    }

    // TODO(dgn): Replace with with shared one after merge.
    public static void waitForWindowUpdates() {
        final long maxWindowUpdateTimeMs = scaleTimeout(1000);
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.waitForWindowUpdate(null, maxWindowUpdateTimeMs);
        device.waitForIdle(maxWindowUpdateTimeMs);
    }
}
