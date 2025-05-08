// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertFalse;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests for TrackingProtectionSettingsBridge. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TrackingProtectionSettingsBridgeTest {
    @ClassRule
    public static final ChromeBrowserTestRule sBrowserTestRule = new ChromeBrowserTestRule();

    private TrackingProtectionSettingsBridge mTrackingProtectionSettingsBridge;

    @Before
    public void setUp() {
        mTrackingProtectionSettingsBridge =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new TrackingProtectionSettingsBridge(
                                        ProfileManager.getLastUsedRegularProfile()));
    }

    @Test
    @SmallTest
    public void isIpProtectionDisabledForEnterpriseIsPresent() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertFalse(
                                mTrackingProtectionSettingsBridge
                                        .isIpProtectionDisabledForEnterprise()));
    }
}
