// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Test suite for quota permissions requests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure
public class QuotaTest {
    @Rule
    public PermissionTestRule mPermissionRule = new PermissionTestRule();

    private static final String TEST_FILE = "/content/test/data/android/quota_permissions.html";

    @Before
    public void setUp() throws Exception {
        mPermissionRule.setUpActivity();
    }

    public QuotaTest() {}

    private void testQuotaPermissionsPlumbing(
            String script, int numUpdates, boolean withGesture, boolean isDialog) throws Exception {
        Tab tab = mPermissionRule.getActivity().getActivityTab();
        PermissionUpdateWaiter updateWaiter =
                new PermissionUpdateWaiter("Count: ", mPermissionRule.getActivity());
        tab.addObserver(updateWaiter);
        mPermissionRule.runAllowTest(
                updateWaiter, TEST_FILE, script, numUpdates, withGesture, isDialog);
        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify asking for quota creates an InfoBar and accepting it resolves the call successfully.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"QuotaPermissions"})
    @DisabledTest(message = "Modals are now enabled and test needs to be reworked crbug.com/935900")
    public void testQuotaShowsInfobar() throws Exception {
        testQuotaPermissionsPlumbing("initiate_requestQuota(1024)", 1, false, false);
    }
}
