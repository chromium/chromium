// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.tab_layout_component;

import static android.support.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.TAB_SELECTION_CALLBACKS;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.support.design.widget.TabLayout;
import android.support.test.filters.MediumTest;
import android.widget.FrameLayout;

import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivity;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View tests for the keyboard accessory tab layout component.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class KeyboardAccessoryTabLayoutViewTest extends DummyUiActivityTestCase {
    private PropertyModel mModel;
    private KeyboardAccessoryTabLayoutView mView;

    private KeyboardAccessoryData.Tab createTestTab(String contentDescription) {
        return new KeyboardAccessoryData.Tab("Passwords",
                getActivity().getResources().getDrawable(android.R.drawable.ic_lock_lock),
                contentDescription,
                R.layout.empty_accessory_sheet, // Unused.
                AccessoryTabType.ALL,
                null); // Unused.
    }

    private CharSequence getTabDescriptionAt(int position) {
        TabLayout.Tab tab = mView.getTabAt(position);
        assert tab != null : "No tab at " + position;
        return tab.getContentDescription();
    }

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        DummyUiActivity.setTestLayout(R.layout.keyboard_accessory_tabs);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        runOnUiThreadBlocking(() -> {
            mModel = new PropertyModel.Builder(TABS, ACTIVE_TAB, TAB_SELECTION_CALLBACKS)
                             .with(TABS, new ListModel<>())
                             .with(ACTIVE_TAB, null)
                             .build();
            mView = (KeyboardAccessoryTabLayoutView) ((FrameLayout) getActivity().findViewById(
                                                              android.R.id.content))
                            .getChildAt(0);
            KeyboardAccessoryTabLayoutCoordinator.createTabViewBinder(mModel, mView);
        });
    }

    @Test
    @MediumTest
    public void testRemovesTabs() {
        runOnUiThreadBlocking(() -> {
            mModel.get(TABS).set(new KeyboardAccessoryData.Tab[] {createTestTab("FirstTab"),
                    createTestTab("SecondTab"), createTestTab("ThirdTab")});
        });

        CriteriaHelper.pollUiThread(() -> mView.getTabCount() == 3);

        assertThat(getTabDescriptionAt(0), is("FirstTab"));
        assertThat(getTabDescriptionAt(1), is("SecondTab"));
        assertThat(getTabDescriptionAt(2), is("ThirdTab"));

        runOnUiThreadBlocking(() -> mModel.get(TABS).remove(mModel.get(TABS).get(1)));

        CriteriaHelper.pollUiThread(() -> mView.getTabCount() == 2);
        assertThat(getTabDescriptionAt(0), is("FirstTab"));
        assertThat(getTabDescriptionAt(1), is("ThirdTab"));
    }

    @Test
    @MediumTest
    public void testAddsTabs() {
        runOnUiThreadBlocking(() -> {
            mModel.get(TABS).set(new KeyboardAccessoryData.Tab[] {
                    createTestTab("FirstTab"), createTestTab("SecondTab")});
        });

        CriteriaHelper.pollUiThread(() -> mView.getTabCount() == 2);
        assertThat(getTabDescriptionAt(0), is("FirstTab"));
        assertThat(getTabDescriptionAt(1), is("SecondTab"));

        runOnUiThreadBlocking(() -> mModel.get(TABS).add(createTestTab("ThirdTab")));

        CriteriaHelper.pollUiThread(() -> mView.getTabCount() == 3);
        assertThat(getTabDescriptionAt(0), is("FirstTab"));
        assertThat(getTabDescriptionAt(1), is("SecondTab"));
        assertThat(getTabDescriptionAt(2), is("ThirdTab"));
    }
}
