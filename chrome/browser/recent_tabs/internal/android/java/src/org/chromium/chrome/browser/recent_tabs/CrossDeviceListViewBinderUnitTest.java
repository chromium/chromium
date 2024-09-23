// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.recent_tabs.CrossDeviceListProperties.ALL_KEYS;
import static org.chromium.chrome.browser.recent_tabs.CrossDeviceListProperties.EMPTY_STATE_VISIBLE;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ui.CrossDevicePaneView;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link CrossDeviceListViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CrossDeviceListViewBinderUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private CrossDevicePaneView mCrossDevicePaneView;
    private ListView mListView;
    private View mEmptyStateContainer;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);

        mListView = mCrossDevicePaneView.findViewById(R.id.cross_device_list_view);
        mEmptyStateContainer = mCrossDevicePaneView.findViewById(R.id.empty_state_container);
        mActivity.setContentView(mCrossDevicePaneView);

        mModel = new PropertyModel.Builder(ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mModel, mCrossDevicePaneView, CrossDeviceListViewBinder::bind);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        mCrossDevicePaneView =
                (CrossDevicePaneView)
                        LayoutInflater.from(mActivity).inflate(R.layout.cross_device_pane, null);
    }

    @Test
    @SmallTest
    public void testEmptyStateVisible() {
        mModel.set(EMPTY_STATE_VISIBLE, true);
        assertEquals(View.VISIBLE, mEmptyStateContainer.getVisibility());
        assertEquals(View.INVISIBLE, mListView.getVisibility());

        mModel.set(EMPTY_STATE_VISIBLE, false);
        assertEquals(View.INVISIBLE, mEmptyStateContainer.getVisibility());
        assertEquals(View.VISIBLE, mListView.getVisibility());
    }
}
