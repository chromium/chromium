// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer.InfoBarContainerObserver;
import org.chromium.chrome.browser.sync.FakeProfileSyncService;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;
import java.util.concurrent.TimeoutException;

/**
 * Test suite for the SyncErrorInfoBar.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncErrorInfoBarTest {
    private static class SyncErrorInfoBarContainerObserver implements InfoBarContainerObserver {
        private CallbackHelper mOnAddInfoBarCallbackHelper = new CallbackHelper();
        private CallbackHelper mOnRemoveInfoBarCallbackHelper = new CallbackHelper();

        @Override
        public void onAddInfoBar(InfoBarContainer container, InfoBar infoBar, boolean isFirst) {
            if (infoBar instanceof SyncErrorInfoBar) mOnAddInfoBarCallbackHelper.notifyCalled();
        }

        @Override
        public void onRemoveInfoBar(InfoBarContainer container, InfoBar infoBar, boolean isLast) {
            if (infoBar instanceof SyncErrorInfoBar) mOnRemoveInfoBarCallbackHelper.notifyCalled();
        }

        @Override
        public void onInfoBarContainerAttachedToWindow(boolean hasInfoBars) {}

        @Override
        public void onInfoBarContainerShownRatioChanged(
                InfoBarContainer container, float shownRatio) {}

        public void waitUntilInfoBarAppears(boolean alreadyShown) throws Exception {
            mOnAddInfoBarCallbackHelper.waitForCallback(alreadyShown ? 1 : 0);
        }

        public void waitUntilInfoBarDisappears() throws Exception {
            mOnRemoveInfoBarCallbackHelper.waitForCallback(0);
        }
    }

    private FakeProfileSyncService mFakeProfileSyncService;
    private InfoBarContainer mInfoBarContainer;
    private SyncErrorInfoBarContainerObserver mInfoBarObserver;

    @Rule
    public final SyncTestRule mSyncTestRule = new SyncTestRule() {
        @Override
        protected FakeProfileSyncService createProfileSyncService() {
            return new FakeProfileSyncService();
        }
    };

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().setRevision(2).build();

    @Before
    public void setUp() {
        deleteSyncErrorInfoBarShowTimePref();
        mFakeProfileSyncService = (FakeProfileSyncService) mSyncTestRule.getProfileSyncService();
        mInfoBarObserver = new SyncErrorInfoBarContainerObserver();
        mInfoBarContainer = mSyncTestRule.getInfoBarContainer();
        mSyncTestRule.getInfoBarContainer().addObserver(mInfoBarObserver);
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForAuthError() throws Exception {
        showSyncErrorInfoBarForAuthError();
        mInfoBarObserver.waitUntilInfoBarAppears(false);

        // Resolving the error should not show the infobar again.
        deleteSyncErrorInfoBarShowTimePref();
        mFakeProfileSyncService.setAuthError(GoogleServiceAuthError.State.NONE);
        mInfoBarObserver.waitUntilInfoBarDisappears();
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForSyncSetupIncomplete() throws Exception {
        showSyncErrorInfoBarForSyncSetupIncomplete();
        mInfoBarObserver.waitUntilInfoBarAppears(false);

        // Resolving the error should not show the infobar again.
        deleteSyncErrorInfoBarShowTimePref();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mFakeProfileSyncService.setFirstSetupComplete(SyncFirstSetupCompleteSource.BASIC_FLOW);
        });
        mInfoBarObserver.waitUntilInfoBarDisappears();
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForPassphraseRequired() throws Exception {
        showSyncErrorInfoBarForPassphraseRequired();
        mInfoBarObserver.waitUntilInfoBarAppears(false);

        // Resolving the error should not show the infobar again.
        deleteSyncErrorInfoBarShowTimePref();
        mFakeProfileSyncService.setPassphraseRequiredForPreferredDataTypes(false);
        mInfoBarObserver.waitUntilInfoBarDisappears();
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForClientOutOfDate() throws Exception {
        showSyncErrorInfoBarForClientOutOfDate();
        mInfoBarObserver.waitUntilInfoBarAppears(false);

        // Not possible to resolve this error from within chrome unlike the other SyncErrorInfoBar
        // types.
    }

    @Test(expected = TimeoutException.class)
    @LargeTest
    public void testSyncErrorInfoBarNotShownWhenNoError() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        mFakeProfileSyncService.setEngineInitialized(true);
        mFakeProfileSyncService.setAuthError(GoogleServiceAuthError.State.NONE);
        mFakeProfileSyncService.setPassphraseRequiredForPreferredDataTypes(false);
        mFakeProfileSyncService.setRequiresClientUpgrade(false);

        @SyncError
        int syncError = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            mFakeProfileSyncService.setFirstSetupComplete(SyncFirstSetupCompleteSource.BASIC_FLOW);
            return SyncSettingsUtils.getSyncError();
        });
        // syncError should not equal to any of these errors that trigger the infobar.
        Assert.assertTrue(syncError != SyncError.AUTH_ERROR);
        Assert.assertTrue(syncError != SyncError.PASSPHRASE_REQUIRED);
        Assert.assertTrue(syncError != SyncError.SYNC_SETUP_INCOMPLETE);
        Assert.assertTrue(syncError != SyncError.CLIENT_OUT_OF_DATE);

        mInfoBarObserver.waitUntilInfoBarAppears(false);
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarIsNotShownBeforeMinimalIntervalPassed() throws Exception {
        showSyncErrorInfoBarForAuthError();
        mInfoBarObserver.waitUntilInfoBarAppears(false);

        // Close the SyncErrorInfoBar and reload the page again.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mSyncTestRule.getInfoBars().get(0).onCloseButtonClicked());
        mSyncTestRule.loadUrl(UrlConstants.CHROME_BLANK_URL);
        Assert.assertEquals(0, mSyncTestRule.getInfoBars().size());
        mInfoBarObserver.waitUntilInfoBarDisappears();

        // Override the time of last seen infobar to minimum required time before current time.
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(SyncErrorInfoBar.PREF_SYNC_ERROR_INFOBAR_SHOWN_AT_TIME,
                        System.currentTimeMillis()
                                - SyncErrorInfoBar.MINIMAL_DURATION_BETWEEN_INFOBARS_MS)
                .apply();
        mSyncTestRule.loadUrl(UrlConstants.CHROME_BLANK_URL);
        mInfoBarObserver.waitUntilInfoBarAppears(true);
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorInfoBarForAuthErrorView() throws IOException {
        showSyncErrorInfoBarForAuthError();
        mRenderTestRule.render(mSyncTestRule.getInfoBarContainer().getContainerViewForTesting(),
                "sync_error_infobar_auth_error");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorInfoBarForSyncSetupIncompleteView() throws IOException {
        showSyncErrorInfoBarForSyncSetupIncomplete();
        mRenderTestRule.render(mSyncTestRule.getInfoBarContainer().getContainerViewForTesting(),
                "sync_error_infobar_sync_setup_incomplete");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorInfoBarForPassphraseRequiredView() throws IOException {
        showSyncErrorInfoBarForPassphraseRequired();
        mRenderTestRule.render(mSyncTestRule.getInfoBarContainer().getContainerViewForTesting(),
                "sync_error_infobar_passphrase_required");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorInfoBarForClientOutOfDateView() throws IOException {
        showSyncErrorInfoBarForClientOutOfDate();
        mRenderTestRule.render(mSyncTestRule.getInfoBarContainer().getContainerViewForTesting(),
                "sync_error_infobar_client_out_of_date");
    }

    private void showSyncErrorInfoBarForAuthError() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeProfileSyncService.setAuthError(GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS);
        mSyncTestRule.loadUrl(UrlConstants.CHROME_BLANK_URL);
    }

    private void showSyncErrorInfoBarForPassphraseRequired() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeProfileSyncService.setEngineInitialized(true);
        mFakeProfileSyncService.setPassphraseRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.CHROME_BLANK_URL);
    }

    private void showSyncErrorInfoBarForSyncSetupIncomplete() {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        mSyncTestRule.loadUrl(UrlConstants.CHROME_BLANK_URL);
    }

    private void showSyncErrorInfoBarForClientOutOfDate() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeProfileSyncService.setRequiresClientUpgrade(true);
        mSyncTestRule.loadUrl(UrlConstants.CHROME_BLANK_URL);
    }

    private void deleteSyncErrorInfoBarShowTimePref() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(SyncErrorInfoBar.PREF_SYNC_ERROR_INFOBAR_SHOWN_AT_TIME)
                .apply();
    }
}
