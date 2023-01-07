// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertThat;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Tests {@link FlocSettingsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class FlocSettingsFragmentTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public SettingsActivityTestRule<FlocSettingsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(FlocSettingsFragment.class);

    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @After
    public void tearDown() {
        if (mActionTester != null) mActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void testCreateActivity() {
        // First start the main activity to initialize metrics.
        mActivityTestRule.startMainActivityOnBlankPage();
        mActionTester = new UserActionTester();
        mSettingsActivityTestRule.startSettingsActivity();
        assertThat(mActionTester.toString(), mActionTester.getActions(),
                Matchers.hasItem("Settings.PrivacySandbox.FlocSubpageOpened"));
    }
}
