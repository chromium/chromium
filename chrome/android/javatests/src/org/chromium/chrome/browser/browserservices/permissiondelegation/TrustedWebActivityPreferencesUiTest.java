// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests for TrustedWebActivity functionality under Settings > Site Settings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
public class TrustedWebActivityPreferencesUiTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private String mPackage;
    private TrustedWebActivityPermissionManager mPermissionMananger;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        mPackage = InstrumentationRegistry.getTargetContext().getPackageName();
        mPermissionMananger = ChromeApplicationImpl.getComponent().resolveTwaPermissionManager();
    }

    /**
     * Tests that the 'Managed by' section appears correctly and that it contains our registered
     * website.
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/1202711")
    public void testSingleCategoryManagedBy() throws Exception {

    }

    /**
     * Tests that registered sites show 'Managed by' in the title when viewing the details for a
     * single website.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testWebsitePreferencesManagedBy() {
    }
}
