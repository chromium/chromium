// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ALL_KEYS;

import android.app.Activity;
import android.view.LayoutInflater;
import android.widget.TextView;

import androidx.core.util.Pair;
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
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link TabGroupRowView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupRowViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock TabGroupTimeAgoResolver mTimeAgoResolver;
    @Mock Runnable mRunnable;

    private Activity mActivity;
    private TabGroupRowView mTabGroupRowView;
    private TextView mTitleTextView;
    private TextView mSubtitleTextView;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity -> mActivity = activity));
    }

    private void remakeWithModel(PropertyModel propertyModel) {
        mPropertyModel = propertyModel;
        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mTabGroupRowView = (TabGroupRowView) inflater.inflate(R.layout.tab_group_row, null, false);
        mTitleTextView = mTabGroupRowView.findViewById(R.id.tab_group_title);
        mSubtitleTextView = mTabGroupRowView.findViewById(R.id.tab_group_subtitle);
        mTabGroupRowView.setTimeAgoResolverForTesting(mTimeAgoResolver);
        PropertyModelChangeProcessor.create(
                mPropertyModel, mTabGroupRowView, new TabGroupRowViewBinder());
    }

    private void remakeWithProperty(ReadableObjectPropertyKey key, Object value) {
        remakeWithModel(new PropertyModel.Builder(ALL_KEYS).with(key, value).build());
    }

    @Test
    @SmallTest
    public void testSetTitleData() {
        remakeWithProperty(TabGroupRowProperties.TITLE_DATA, new Pair<>("Title", 3));
        assertEquals("Title", mTitleTextView.getText());

        remakeWithProperty(TabGroupRowProperties.TITLE_DATA, new Pair<>(" ", 3));
        assertEquals(" ", mTitleTextView.getText());

        remakeWithProperty(TabGroupRowProperties.TITLE_DATA, new Pair<>("", 3));
        assertEquals("3 tabs", mTitleTextView.getText());

        remakeWithProperty(TabGroupRowProperties.TITLE_DATA, new Pair<>(null, 3));
        assertEquals("3 tabs", mTitleTextView.getText());

        remakeWithProperty(TabGroupRowProperties.TITLE_DATA, new Pair<>("", 1));
        assertEquals("1 tab", mTitleTextView.getText());
    }

    @Test
    @SmallTest
    public void testSetCreationMillis() {
        long creationMillis = 123L;
        String timeAgo = "Created just now";
        when(mTimeAgoResolver.resolveTimeAgoText(creationMillis)).thenReturn(timeAgo);

        remakeWithProperty(TabGroupRowProperties.CREATION_MILLIS, creationMillis);

        verify(mTimeAgoResolver).resolveTimeAgoText(creationMillis);
        assertEquals(timeAgo, mSubtitleTextView.getText());
    }

    @Test
    @SmallTest
    public void testSetOpenRunnable() {
        remakeWithProperty(TabGroupRowProperties.OPEN_RUNNABLE, mRunnable);
        mTabGroupRowView.performClick();
        verify(mRunnable).run();

        reset(mRunnable);
        remakeWithProperty(TabGroupRowProperties.OPEN_RUNNABLE, null);
        mTabGroupRowView.performClick();
        verifyNoInteractions(mRunnable);
    }
}
