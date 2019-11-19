// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.sync.FakeProfileSyncService;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Instrumentation test for Identity Disc.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IdentityDiscControllerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    // IdentityDiscController subscribes for sync state change events right after native
    // initialization. Currently there is no facility to reliably substitute ProfileSyncService
    // for fake one from test before IdentityDiscController gets a reference to it.
    // We keep reference to FakeProfileSyncService to affect logic based on sync state. Reference to
    // original ProfileSyncService is used for triggering sync state change notifications.
    private FakeProfileSyncService mFakeProfileSyncService;
    private ProfileSyncService mOriginalProfileSyncService;

    @Before
    public void setUp() {
        SigninTestUtil.setUpAuthForTest();
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mOriginalProfileSyncService = ProfileSyncService.get();
            mFakeProfileSyncService = new FakeProfileSyncService();
            ProfileSyncService.overrideForTests(mFakeProfileSyncService);
        });
    }

    @After
    public void tearDown() {
        SigninTestUtil.tearDownAuthForTest();
    }

    @Test
    @MediumTest
    public void testIdentityDiscWithNavigation() {
        // User is signed in and syncing.
        SigninTestUtil.addAndSignInTestAccount();
        mFakeProfileSyncService.setCanSyncFeatureStart(true);

        // Identity Disc should be visible on NTP.
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        View experimentalButton =
                mActivityTestRule.getActivity().findViewById(R.id.experimental_toolbar_button);
        Assert.assertNotNull("IdentityDisc is not inflated", experimentalButton);
        Assert.assertEquals(
                "IdentityDisc is not visible", View.VISIBLE, experimentalButton.getVisibility());

        // Identity Disc should be hidden on navigation away from NTP.
        mActivityTestRule.loadUrl(UrlConstants.CHROME_BLANK_URL);
        Assert.assertEquals("IdentityDisc is still visible outside of NTP", View.GONE,
                experimentalButton.getVisibility());
    }

    @Test
    @MediumTest
    public void testIdentityDiscWithSyncState() {
        SigninTestUtil.addAndSignInTestAccount();
        mFakeProfileSyncService.setCanSyncFeatureStart(false);

        // When sync is disabled Identity Disc should not be visible.
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        View experimentalButton =
                mActivityTestRule.getActivity().findViewById(R.id.experimental_toolbar_button);
        Assert.assertNull("IdentityDisc is visible when sync is disabled", experimentalButton);

        // Identity Disc should be shown on sync state change without NTP refresh.
        mFakeProfileSyncService.setCanSyncFeatureStart(true);
        triggerSyncStateChange();
        experimentalButton =
                mActivityTestRule.getActivity().findViewById(R.id.experimental_toolbar_button);
        Assert.assertNotNull("IdentityDisc is not inflated", experimentalButton);
        Assert.assertEquals(
                "IdentityDisc is not visible", View.VISIBLE, experimentalButton.getVisibility());
    }

    @Test
    @MediumTest
    public void testIdentityDiscWithSignInState() {
        // When user is signed out, Identity Disc should not be visible.
        mFakeProfileSyncService.setCanSyncFeatureStart(true);
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        View experimentalButton =
                mActivityTestRule.getActivity().findViewById(R.id.experimental_toolbar_button);
        Assert.assertNull("IdentityDisc is visible for signed out user", experimentalButton);

        // Identity Disc should be shown on sign-in state change without NTP refresh.
        SigninTestUtil.addAndSignInTestAccount();
        triggerSyncStateChange();
        experimentalButton =
                mActivityTestRule.getActivity().findViewById(R.id.experimental_toolbar_button);
        Assert.assertNotNull("IdentityDisc is not inflated", experimentalButton);
        Assert.assertEquals(
                "IdentityDisc is not visible", View.VISIBLE, experimentalButton.getVisibility());
    }

    private void triggerSyncStateChange() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mOriginalProfileSyncService.syncStateChanged(); });
    }
}
