// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;

/**
 * Unit tests for RestoreTabsCoordinator.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsCoordinatorUnitTest {
    @Mock
    private RestoreTabsMediator mMediator;
    @Mock
    private Profile mProfile;
    @Mock
    private RestoreTabsControllerFactory.ControllerListener mListener;
    @Mock
    private TabCreatorManager mTabCreatorManager;

    private RestoreTabsCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mCoordinator =
                new RestoreTabsCoordinator(mProfile, mMediator, mListener, mTabCreatorManager);
    }

    @Test
    public void testRestoreTabsCoordinator_showOptions() {
        mCoordinator.showOptions();
        verify(mMediator, times(1)).showOptions();
    }
}
