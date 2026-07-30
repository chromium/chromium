// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.junit.Assert.assertNotNull;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.util.List;

/** Tests for PrivacySandboxBridge. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=PrivacySandboxSettings4:show-sample-data/true"
})
@Batch(Batch.PER_CLASS)
public class PrivacySandboxBridgeTest {
    private PrivacySandboxBridge mPrivacySandboxBridge;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mPrivacySandboxBridge =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new PrivacySandboxBridge(ProfileManager.getLastUsedRegularProfile()));
    }

    @Nullable
    private List<String> getFledgeJoiningEtlds() {
        PayloadCallbackHelper<List<String>> callbackHelper = new PayloadCallbackHelper<>();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPrivacySandboxBridge.getFledgeJoiningEtldPlusOneForDisplay(
                                callbackHelper::notifyCalled));
        return callbackHelper.getOnlyPayloadBlocking();
    }

    @Test
    @SmallTest
    public void testGetFledgeJoiningEtldPlusOneForDisplay() {
        // Check that this function returns a valid list. We currently can't control from the Java
        // side what they actually return, so just check that it is not null and there is no crash.
        assertNotNull(getFledgeJoiningEtlds());
    }

    @Test
    @SmallTest
    public void testGetBlockedFledgeJoiningTopFramesForDisplay() {
        // Check that this function returns a valid list. We currently can't control from the Java
        // side what they actually return, so just check that it is not null and there is no crash.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertNotNull(
                                mPrivacySandboxBridge
                                        .getBlockedFledgeJoiningTopFramesForDisplay()));
    }

    @Test
    @SmallTest
    public void testFledgeBlocking() {
        String site1 = "a.com";
        String site2 = "b.com";
        String site3 = "c.com";

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPrivacySandboxBridge.setFledgeJoiningAllowed(site1, false);
                    assertThat(
                            mPrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay(),
                            contains(site1));

                    mPrivacySandboxBridge.setFledgeJoiningAllowed(site2, false);
                    mPrivacySandboxBridge.setFledgeJoiningAllowed(site3, false);
                    assertThat(
                            mPrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay(),
                            contains(site1, site2, site3));

                    mPrivacySandboxBridge.setFledgeJoiningAllowed(site2, true);
                    assertThat(
                            mPrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay(),
                            contains(site1, site3));
                });
    }
}
