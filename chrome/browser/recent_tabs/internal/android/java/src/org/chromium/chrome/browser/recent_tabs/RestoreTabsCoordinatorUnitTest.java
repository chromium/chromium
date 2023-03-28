// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for RestoreTabsCoordinator.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsCoordinatorUnitTest {
    @Mock
    private RestoreTabsMediator mMediator;
    @Mock
    private ForeignSessionHelper mForeignSessionHelper;

    private RestoreTabsCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mCoordinator = new RestoreTabsCoordinator(mForeignSessionHelper, mMediator,
                new RestoreTabsControllerFactory.ControllerListener() {
                    @Override
                    public void onDismissed() {
                        mCoordinator.destroy();
                    }
                });
    }

    @Test
    public void testRestoreTabsCoordinator_showOptions() {
        ForeignSession session = new ForeignSession("tag", "John's iPhone 6", 32L);
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session);

        when(mForeignSessionHelper.getForeignSessions()).thenReturn(testSessions);
        mCoordinator.showOptions();
        verify(mMediator, times(1)).showOptions(testSessions);
    }
}
