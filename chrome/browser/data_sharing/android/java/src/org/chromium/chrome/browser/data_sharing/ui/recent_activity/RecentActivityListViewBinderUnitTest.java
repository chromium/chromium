// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@RunWith(BaseRobolectricTestRunner.class)
public class RecentActivityListViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private ViewGroup mListView;
    private ViewGroup mListItemView1;
    private PropertyModel mPropertyModel;
    @Mock private OnClickListener mOnClickListener;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        // Inflate main view.
        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mListView =
                (ViewGroup) inflater.inflate(R.layout.recent_activity_bottom_sheet, null, false);
        mActivity.setContentView(mListView);

        // Add one row.
        mListItemView1 =
                (ViewGroup) inflater.inflate(R.layout.recent_activity_log_item, null, false);
        ((ViewGroup) mListView.findViewById(R.id.recent_activity_recycler_view))
                .addView(mListItemView1);

        mPropertyModel = new PropertyModel.Builder(RecentActivityListProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mListItemView1, RecentActivityListViewBinder::bind);
    }

    @Test
    public void testTitle() {
        final String title = "Title 1";
        mPropertyModel.set(RecentActivityListProperties.TITLE_TEXT, title);
        TextView textView = mListItemView1.findViewById(R.id.title);
        Assert.assertEquals(textView.getText(), title);
    }

    @Test
    public void testDescription() {
        final String description = "Description 1";
        mPropertyModel.set(RecentActivityListProperties.DESCRIPTION_TEXT, description);
        TextView textView = mListItemView1.findViewById(R.id.description);
        Assert.assertEquals(textView.getText(), description);
    }

    @Test
    public void testOnClickListener() {
        mPropertyModel.set(RecentActivityListProperties.ON_CLICK_LISTENER, mOnClickListener);
        mListItemView1.performClick();
        verify(mOnClickListener).onClick(any());
    }
}
