// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.LayoutInflater;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator.StreamTabId;
import org.chromium.chrome.browser.feed.test.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.Tracker;

/** Test for the WebFeedFollowIntroView class. */
@RunWith(BaseRobolectricTestRunner.class)
public final class SectionHeaderViewTest {
    private static final String TAG = "SectionHeaderViewTst";
    private SectionHeaderView mSectionHeaderView;
    private Activity mActivity;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private Tracker mTracker;
    @Mock
    private UserEducationHelper mHelper;
    @Mock
    Runnable mScroller;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(R.style.Theme_MaterialComponents);
        TrackerFactory.setTrackerForTests(mTracker);

        // Build the class under test, and set up the fake UI.
        mSectionHeaderView = (SectionHeaderView) LayoutInflater.from(mActivity).inflate(
                R.layout.new_tab_page_multi_feed_header, null, false);
        mSectionHeaderView.addTab();
        mSectionHeaderView.addTab();
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    private void setFeatureOverridesForIPH() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.ANDROID_SCROLL_OPTIMIZATIONS, false);
        testValues.addFeatureFlagOverride(ChromeFeatureList.WEB_FEED, true);
        testValues.addFeatureFlagOverride(ChromeFeatureList.WEB_FEED_ONBOARDING, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style", "IPH");
        FeatureList.setTestValues(testValues);
    }

    @Test
    @SmallTest
    public void showWebFeedIPHTest() {
        setFeatureOverridesForIPH();
        mSectionHeaderView.showWebFeedAwarenessIph(mHelper, StreamTabId.FOLLOWING, mScroller);
        verify(mHelper, times(1)).requestShowIPH(any());
    }
}
