// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link TabStripViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabStripViewBinderUnitTest {
    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private ViewGroup mStripItemView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mStripItemView =
                (ViewGroup)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.tab_strip_item, /* root= */ null);
        mModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_STRIP)
                        .with(TabProperties.TITLE, "")
                        .build();
        PropertyModelChangeProcessor.create(mModel, mStripItemView, TabStripViewBinder::bind);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.DATA_SHARING, ChromeFeatureList.DATA_SHARING_JOIN_ONLY})
    public void testTabStripItemTitleUpdatesContentDescription() {
        View button = mStripItemView.findViewById(R.id.tab_strip_item_button);
        assertEquals(
                mActivity.getString(R.string.accessibility_tabstrip_tab, ""),
                button.getContentDescription());

        mModel.set(TabProperties.TITLE, "New Title");
        assertEquals(
                mActivity.getString(R.string.accessibility_tabstrip_tab, "New Title"),
                button.getContentDescription());

        mModel.set(TabProperties.IS_SELECTED, true);
        mModel.set(TabProperties.TITLE, "Selected Title");
        assertEquals(
                mActivity.getString(
                        R.string.accessibility_tabstrip_btn_close_tab, "Selected Title"),
                button.getContentDescription());
    }
}
