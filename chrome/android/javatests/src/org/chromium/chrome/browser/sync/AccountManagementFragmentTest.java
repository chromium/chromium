// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccountManagementFragmentTest {
    @Rule
    public final SettingsActivityTestRule<AccountManagementFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AccountManagementFragment.class);

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        mSettingsActivityTestRule.startSettingsActivity();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void testAccountManagementFragmentViewLegacy() throws Exception {
        mRenderTestRule.render(mSettingsActivityTestRule.getFragment().getView(),
                "account_management_fragment_view_legacy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void testAccountManagementFragmentView() throws Exception {
        mRenderTestRule.render(mSettingsActivityTestRule.getFragment().getView(),
                "account_management_fragment_view");
    }
}
