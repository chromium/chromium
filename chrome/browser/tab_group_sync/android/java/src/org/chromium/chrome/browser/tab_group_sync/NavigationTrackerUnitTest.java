// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the {@link NavigationTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NavigationTrackerUnitTest {
    private NavigationTracker mNavigationTracker = new NavigationTracker();

    @Test
    public void testSyncInitiatedNavigation() {
        UserDataHost userDataHost = new UserDataHost();
        mNavigationTracker.setNavigationWasFromSync(userDataHost);
        Assert.assertTrue(mNavigationTracker.wasNavigationFromSync(userDataHost));
    }

    @Test
    public void testNotSyncInitiatedNavigation() {
        UserDataHost userDataHost = new UserDataHost();
        Assert.assertFalse(mNavigationTracker.wasNavigationFromSync(userDataHost));
    }

    @Test
    public void testTrackingResetAfterQuery() {
        UserDataHost userDataHost = new UserDataHost();
        mNavigationTracker.setNavigationWasFromSync(userDataHost);
        Assert.assertTrue(mNavigationTracker.wasNavigationFromSync(userDataHost));
        Assert.assertFalse(mNavigationTracker.wasNavigationFromSync(userDataHost));
    }
}
