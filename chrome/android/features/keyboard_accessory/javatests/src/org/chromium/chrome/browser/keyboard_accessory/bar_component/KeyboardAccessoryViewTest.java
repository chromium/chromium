// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY;
import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.GENERATE_PASSWORD_AUTOMATIC;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISABLE_ANIMATIONS_FOR_TESTING;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_OPENER_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_INVISIBLE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SheetOpenerBarItem;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.AsyncViewProvider;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.ViewUtils;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.atomic.AtomicReference;

/** View tests for the keyboard accessory component. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
public class KeyboardAccessoryViewTest {
    private PropertyModel mModel;
    private BlockingQueue<KeyboardAccessoryView> mKeyboardAccessoryView;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel =
                            KeyboardAccessoryProperties.defaultModelBuilder()
                                    .with(
                                            SHEET_OPENER_ITEM,
                                            new SheetOpenerBarItem(
                                                    new KeyboardAccessoryTabLayoutCoordinator
                                                            .SheetOpenerCallbacks() {
                                                        @Override
                                                        public void onViewBound(View tabs) {}

                                                        @Override
                                                        public void onViewUnbound(View tabs) {}
                                                    }))
                                    .with(DISABLE_ANIMATIONS_FOR_TESTING, true)
                                    .build();
                    AsyncViewStub viewStub =
                            mActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.keyboard_accessory_stub);

                    mKeyboardAccessoryView = new ArrayBlockingQueue<>(1);
                    ViewProvider<KeyboardAccessoryView> provider =
                            AsyncViewProvider.of(viewStub, R.id.keyboard_accessory);
                    LazyConstructionPropertyMcp.create(
                            mModel, VISIBLE, provider, KeyboardAccessoryViewBinder::bind);
                    provider.whenLoaded(mKeyboardAccessoryView::add);
                });
    }

    @Test
    @MediumTest
    public void testAccessoryVisibilityChangedByModel() throws InterruptedException {
        // Initially, there shouldn't be a view yet.
        assertNull(mKeyboardAccessoryView.poll());

        // After setting the visibility to true, the view should exist and be visible.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                });
        KeyboardAccessoryView view = mKeyboardAccessoryView.take();
        assertEquals(view.getVisibility(), View.VISIBLE);

        // After hiding the view, the view should still exist but be invisible.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, false);
                });
        assertNotEquals(view.getVisibility(), View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testClickableActionAddedWhenChangingModel() {
        final AtomicReference<Boolean> buttonClicked = new AtomicReference<>();
        final BarItem testItem =
                new BarItem(
                        BarItem.Type.ACTION_BUTTON,
                        new Action(GENERATE_PASSWORD_AUTOMATIC, action -> buttonClicked.set(true)),
                        R.string.password_generation_accessory_button);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).add(testItem);
                });

        onViewWaiting(withText(R.string.password_generation_accessory_button)).perform(click());

        assertTrue(buttonClicked.get());
    }

    @Test
    @MediumTest
    public void testCanAddSingleButtons() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS)
                            .set(
                                    new BarItem[] {
                                        new BarItem(
                                                BarItem.Type.ACTION_BUTTON,
                                                new Action(
                                                        GENERATE_PASSWORD_AUTOMATIC, action -> {}),
                                                R.string.password_generation_accessory_button)
                                    });
                });

        onViewWaiting(withText(R.string.password_generation_accessory_button));
        onView(withText(R.string.password_generation_accessory_button))
                .check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(BAR_ITEMS)
                            .add(
                                    new BarItem(
                                            BarItem.Type.ACTION_CHIP,
                                            new Action(
                                                    CREDMAN_CONDITIONAL_UI_REENTRY, action -> {}),
                                            R.string.more_passkeys));
                });

        onViewWaiting(withText(R.string.more_passkeys));
        onView(withText(R.string.password_generation_accessory_button))
                .check(matches(isDisplayed()));
        onView(withText(R.string.more_passkeys)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testCanRemoveSingleButtons() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS)
                            .set(
                                    new BarItem[] {
                                        new BarItem(
                                                BarItem.Type.ACTION_BUTTON,
                                                new Action(
                                                        GENERATE_PASSWORD_AUTOMATIC, action -> {}),
                                                R.string.password_generation_accessory_button),
                                        new BarItem(
                                                BarItem.Type.ACTION_CHIP,
                                                new Action(
                                                        CREDMAN_CONDITIONAL_UI_REENTRY,
                                                        action -> {}),
                                                R.string.more_passkeys),
                                    });
                });

        onViewWaiting(withText(R.string.password_generation_accessory_button));
        onView(withText(R.string.password_generation_accessory_button))
                .check(matches(isDisplayed()));
        onView(withText(R.string.more_passkeys)).check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.get(BAR_ITEMS).remove(mModel.get(BAR_ITEMS).get(1)));

        ViewUtils.waitForViewCheckingState(
                withText(R.string.more_passkeys), VIEW_INVISIBLE | VIEW_GONE | VIEW_NULL);
        onView(withText(R.string.password_generation_accessory_button))
                .check(matches(isDisplayed()));
        onView(withText(R.string.more_passkeys)).check(doesNotExist());
    }
}
