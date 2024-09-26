// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListProperties.EMPTY_STATE_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListProperties.ON_IS_SCROLLED_CHANGED;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListProperties.SYNC_ENABLED;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.core.util.Consumer;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link TabGroupListView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupListViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Consumer<Boolean> mOnIsScrolledConsumer;

    private Activity mActivity;
    private TabGroupListView mTabGroupListView;
    private RecyclerView mRecyclerView;
    private View mEmptyStateContainer;
    private TextView mEmptyStateSubheading;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mTabGroupListView =
                (TabGroupListView) inflater.inflate(R.layout.tab_group_list, null, false);
        mRecyclerView = mTabGroupListView.findViewById(R.id.tab_group_list_recycler_view);
        mEmptyStateContainer = mTabGroupListView.findViewById(R.id.empty_state_container);
        mEmptyStateSubheading = mTabGroupListView.findViewById(R.id.empty_state_text_description);
        mActivity.setContentView(mTabGroupListView);

        mPropertyModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(ON_IS_SCROLLED_CHANGED, mOnIsScrolledConsumer)
                        .build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mTabGroupListView, TabGroupListViewBinder::bind);
    }

    @Test
    @SmallTest
    public void testEmptyStateVisible() {
        mPropertyModel.set(EMPTY_STATE_VISIBLE, true);
        assertEquals(View.VISIBLE, mEmptyStateContainer.getVisibility());
        assertEquals(View.INVISIBLE, mRecyclerView.getVisibility());

        mPropertyModel.set(EMPTY_STATE_VISIBLE, false);
        assertEquals(View.INVISIBLE, mEmptyStateContainer.getVisibility());
        assertEquals(View.VISIBLE, mRecyclerView.getVisibility());
    }

    @Test
    @SmallTest
    public void testSyncEnabled() {
        mPropertyModel.set(SYNC_ENABLED, true);
        CharSequence enabledString = mEmptyStateSubheading.getText();

        mPropertyModel.set(SYNC_ENABLED, false);
        CharSequence disabledString = mEmptyStateSubheading.getText();
        assertNotEquals(enabledString, disabledString);
    }
}
