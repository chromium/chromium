// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.res.Resources;

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
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/** Unit tests for {@link TabGroupSyncIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.TAB_STRIP_GROUP_COLLAPSE,
    ChromeFeatureList.TAB_GROUP_SYNC_ANDROID
})
public class TabGroupSyncIphControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Resources mResources;
    @Mock private Tracker mTracker;
    private TabGroupSyncIphController mController;

    @Before
    public void setUp() {
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUI(FeatureConstants.TAB_GROUP_SYNC_ON_STRIP_FEATURE))
                .thenReturn(true);
        TrackerFactory.setTrackerForTests(mTracker);
        mController = new TabGroupSyncIphController(mResources, mUserEducationHelper, 0, mTracker);
    }

    @Test
    public void testRequestShowIPH() {
        mController.maybeShowIphOnTabStrip(null, 0.f, 0.f, 0.f, 0.f);
        var captor = ArgumentCaptor.forClass(IPHCommand.class);
        verify(mUserEducationHelper).requestShowIPH(captor.capture());
        var cmd = captor.getValue();
        assertEquals(FeatureConstants.TAB_GROUP_SYNC_ON_STRIP_FEATURE, cmd.featureName);
    }
}
