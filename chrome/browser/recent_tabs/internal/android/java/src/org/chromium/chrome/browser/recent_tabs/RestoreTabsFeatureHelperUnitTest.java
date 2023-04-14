// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Unit tests for RestoreTabsFeatureHelper.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsFeatureHelperUnitTest {
    private RestoreTabsControllerImpl mController;
    private RestoreTabsFeatureHelper mHelper;

    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Mock
    ForeignSessionHelper.Natives mForeignSessionHelperJniMock;
    @Mock
    private Profile mProfile;
    @Mock
    private Tracker mTracker;
    @Mock
    private RestoreTabsControllerFactory.ControllerListener mListener;
    @Mock
    private TabCreatorManager mTabCreatorManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfile);
        TrackerFactory.setTrackerForTests(mTracker);
        jniMocker.mock(ForeignSessionHelperJni.TEST_HOOKS, mForeignSessionHelperJniMock);

        mController = RestoreTabsControllerFactory.createInstance(
                mProfile, mListener, mTabCreatorManager);
        mHelper = mController.getFeatureHelper();
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @SmallTest
    public void testFeatureHelper_ConfigureOnFirstRun() {
        mHelper.configureOnFirstRun(Profile.getLastUsedRegularProfile());
        verify(mTracker, times(1)).notifyEvent(EventConstants.RESTORE_TABS_ON_FIRST_RUN_SHOW_PROMO);
    }
}
