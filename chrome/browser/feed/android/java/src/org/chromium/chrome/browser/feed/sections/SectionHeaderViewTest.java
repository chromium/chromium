// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator.StreamTabId;
import org.chromium.chrome.browser.feed.test.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.Tracker;

/** Test for the WebFeedFollowIntroView class. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({ChromeFeatureList.FEED_CONTAINMENT})
public final class SectionHeaderViewTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private SectionHeaderView mSectionHeaderView;
    private Activity mActivity;

    @Mock private Tracker mTracker;
    @Mock private UserEducationHelper mHelper;
    @Mock Runnable mScroller;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        TrackerFactory.setTrackerForTests(mTracker);

        // Build the class under test, and set up the fake UI.
        mSectionHeaderView =
                (SectionHeaderView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.new_tab_page_multi_feed_header, null, false);
        ViewGroup contentView = new LinearLayout(mActivity);
        mActivity.setContentView(contentView);
        contentView.addView(mSectionHeaderView);

        mSectionHeaderView.addTab();
        mSectionHeaderView.addTab();
    }

    private void setFeatureOverridesForIph() {
        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.WEB_FEED_ONBOARDING)
                .param(ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style", "IPH")
                .apply();
    }

    @Test
    @SmallTest
    public void showWebFeedIphTest() {
        setFeatureOverridesForIph();
        mSectionHeaderView.showWebFeedAwarenessIph(mHelper, StreamTabId.FOLLOWING, mScroller);
        verify(mHelper, times(1)).requestShowIph(any());
    }

    @Test
    @SmallTest
    public void mainContentTopMarginTest() {
        mSectionHeaderView.onFinishInflate();

        View mainContentView =
                mSectionHeaderView.findViewById(org.chromium.chrome.browser.feed.R.id.main_content);
        MarginLayoutParams contentMarginLayoutParams =
                (MarginLayoutParams) mainContentView.getLayoutParams();
        Assert.assertEquals(
                mSectionHeaderView
                        .getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.browser.feed.R.dimen.feed_header_top_margin),
                contentMarginLayoutParams.topMargin);
    }
}
