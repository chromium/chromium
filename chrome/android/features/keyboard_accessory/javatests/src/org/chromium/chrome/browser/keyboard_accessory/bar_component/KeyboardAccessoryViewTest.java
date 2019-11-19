// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.AUTOFILL_SUGGESTION;
import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.GENERATE_PASSWORD_AUTOMATIC;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISABLE_ANIMATIONS_FOR_TESTING;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.TAB_LAYOUT_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_INVISIBLE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.support.design.widget.TabLayout;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.TabLayoutBarItem;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.DeferredViewStubInflationProvider;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.atomic.AtomicReference;

/**
 * View tests for the keyboard accessory component.
 *
 */
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel =
                    KeyboardAccessoryProperties.defaultModelBuilder()
                            .with(TAB_LAYOUT_ITEM,
                                    new TabLayoutBarItem(new KeyboardAccessoryTabLayoutCoordinator
                                                                 .TabLayoutCallbacks() {
                                                                     @Override
                                                                     public void onTabLayoutBound(
                                                                             TabLayout tabs) {}
                                                                     @Override
                                                                     public void onTabLayoutUnbound(
                                                                             TabLayout tabs) {}
                                                                 }))
                            .with(DISABLE_ANIMATIONS_FOR_TESTING, true)
                            .build();
            ViewStub viewStub =
                    mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory_stub);

            mKeyboardAccessoryView = new ArrayBlockingQueue<>(1);
            ViewProvider<KeyboardAccessoryView> provider =
                    new DeferredViewStubInflationProvider<>(viewStub);
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
        TestThreadUtils.runOnUiThreadBlocking(() -> { mModel.set(VISIBLE, true); });
        KeyboardAccessoryView view = mKeyboardAccessoryView.take();
        assertEquals(view.getVisibility(), View.VISIBLE);

        // After hiding the view, the view should still exist but be invisible.
        TestThreadUtils.runOnUiThreadBlocking(() -> { mModel.set(VISIBLE, false); });
        assertNotEquals(view.getVisibility(), View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testClickableActionAddedWhenChangingModel() {
        final AtomicReference<Boolean> buttonClicked = new AtomicReference<>();
        final BarItem testItem = new BarItem(BarItem.Type.ACTION_BUTTON,
                new Action("Test Button", GENERATE_PASSWORD_AUTOMATIC,
                        action -> buttonClicked.set(true)));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).add(testItem);
        });

        onView(isRoot()).check((root, e) -> waitForView((ViewGroup) root, withText("Test Button")));
        onView(withText("Test Button")).perform(click());

        assertTrue(buttonClicked.get());
    }

    @Test
    @MediumTest
    public void testCanAddSingleButtons() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(new BarItem[] {
                    new BarItem(BarItem.Type.ACTION_BUTTON,
                            new Action("First", GENERATE_PASSWORD_AUTOMATIC, action -> {})),
                    new BarItem(BarItem.Type.SUGGESTION,
                            new Action("Second", AUTOFILL_SUGGESTION, action -> {}))});
        });

        onView(isRoot()).check((root, e) -> waitForView((ViewGroup) root, withText("First")));
        onView(withText("First")).check(matches(isDisplayed()));
        onView(withText("Second")).check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(BAR_ITEMS).add(new BarItem(BarItem.Type.ACTION_BUTTON,
                    new Action("Third", GENERATE_PASSWORD_AUTOMATIC, action -> {})));
        });

        onView(isRoot()).check((root, e) -> waitForView((ViewGroup) root, withText("Third")));
        onView(withText("First")).check(matches(isDisplayed()));
        onView(withText("Second")).check(matches(isDisplayed()));
        onView(withText("Third")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testCanRemoveSingleButtons() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(new BarItem[] {
                    new BarItem(BarItem.Type.ACTION_BUTTON,
                            new Action("First", GENERATE_PASSWORD_AUTOMATIC, action -> {})),
                    new BarItem(BarItem.Type.ACTION_BUTTON,
                            new Action("Second", GENERATE_PASSWORD_AUTOMATIC, action -> {})),
                    new BarItem(BarItem.Type.ACTION_BUTTON,
                            new Action("Third", GENERATE_PASSWORD_AUTOMATIC, action -> {}))});
        });

        onView(isRoot()).check((root, e) -> waitForView((ViewGroup) root, withText("First")));
        onView(withText("First")).check(matches(isDisplayed()));
        onView(withText("Second")).check(matches(isDisplayed()));
        onView(withText("Third")).check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.get(BAR_ITEMS).remove(mModel.get(BAR_ITEMS).get(1)));

        onView(isRoot()).check((root, e)
                                       -> waitForView((ViewGroup) root, withText("Second"),
                                               VIEW_INVISIBLE | VIEW_GONE | VIEW_NULL));
        onView(withText("First")).check(matches(isDisplayed()));
        onView(withText("Second")).check(doesNotExist());
        onView(withText("Third")).check(matches(isDisplayed()));
    }
}