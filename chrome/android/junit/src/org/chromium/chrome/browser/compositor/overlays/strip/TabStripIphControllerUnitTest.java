// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripIphController.IphType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/** Unit tests for {@link TabStripIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.TAB_STRIP_GROUP_COLLAPSE,
    ChromeFeatureList.TAB_GROUP_SYNC_ANDROID
})
public class TabStripIphControllerUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Resources mResources;
    @Mock private Tracker mTracker;
    @Mock private View mContainerView;
    @Mock private StripLayoutGroupTitle mGroupTitle;
    private TabStripIphController mController;

    @Before
    public void setUp() {
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.TAB_GROUP_SYNC_ON_STRIP_FEATURE))
                .thenReturn(true);
        TrackerFactory.setTrackerForTests(mTracker);
        mController = new TabStripIphController(mResources, mUserEducationHelper, mTracker);
        DisplayMetrics displayMetrics = new DisplayMetrics();
        displayMetrics.density = 1.f;
        when(mResources.getDisplayMetrics()).thenReturn(displayMetrics);
    }

    @Test
    public void testRequestShowIph() {
        mController.maybeShowIphOnTabStrip(mGroupTitle, mContainerView, IphType.TAB_GROUP_SYNC, 0f);
        var captor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper).requestShowIph(captor.capture());
        var cmd = captor.getValue();
        assertEquals(FeatureConstants.TAB_GROUP_SYNC_ON_STRIP_FEATURE, cmd.featureName);
    }
}
