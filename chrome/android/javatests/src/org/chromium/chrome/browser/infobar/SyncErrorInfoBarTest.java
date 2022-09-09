// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SYNC_ERROR_PROMPT_SHOWN_AT_TIME;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer.InfoBarContainerObserver;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.browser.sync.ui.SyncErrorPromptUtils;
import org.chromium.chrome.browser.sync.ui.SyncErrorPromptUtils.SyncErrorPromptType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
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
@DisableFeatures({ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE})
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

    private FakeSyncServiceImpl mFakeSyncServiceImpl;
    private SyncErrorInfoBarContainerObserver mInfoBarObserver;

    @Rule
    public final SyncTestRule mSyncTestRule = new SyncTestRule() {
        @Override
        protected FakeSyncServiceImpl createSyncServiceImpl() {
            return new FakeSyncServiceImpl();
        }
    };

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SYNC)
                    .build();

    @Before
    public void setUp() {
        SyncErrorPromptUtils.resetLastShownTime();
        mFakeSyncServiceImpl = (FakeSyncServiceImpl) mSyncTestRule.getSyncService();
        mInfoBarObserver = new SyncErrorInfoBarContainerObserver();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mSyncTestRule.getInfoBarContainer().addObserver(mInfoBarObserver));
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForAuthError() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setAuthError(GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        mInfoBarObserver.waitUntilInfoBarAppears(false);

        // Resolving the error should hide the infobar.
        mFakeSyncServiceImpl.setAuthError(GoogleServiceAuthError.State.NONE);
        mInfoBarObserver.waitUntilInfoBarDisappears();
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForSyncSetupIncomplete() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        mInfoBarObserver.waitUntilInfoBarAppears(false);

        // Resolving the error should hide the infobar.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mFakeSyncServiceImpl.setFirstSetupComplete(SyncFirstSetupCompleteSource.BASIC_FLOW);
        });
        mInfoBarObserver.waitUntilInfoBarDisappears();
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForPassphraseRequired() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        mInfoBarObserver.waitUntilInfoBarAppears(false);

        // Resolving the error should hide the infobar.
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(false);
        mInfoBarObserver.waitUntilInfoBarDisappears();
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForClientOutOfDate() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setRequiresClientUpgrade(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        mInfoBarObserver.waitUntilInfoBarAppears(false);

        // Not possible to resolve this error from within chrome unlike the other
        // SyncErrorPromptType-s.
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForTrustedVaultKeyRequired() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        mInfoBarObserver.waitUntilInfoBarAppears(false);

        // Resolving the error should hide the infobar.
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(false);
        mInfoBarObserver.waitUntilInfoBarDisappears();
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForTrustedVaultRecoverabilityDegraded() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultRecoverabilityDegraded(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        mInfoBarObserver.waitUntilInfoBarAppears(false);

        // Resolving the error should hide the infobar.
        mFakeSyncServiceImpl.setTrustedVaultRecoverabilityDegraded(false);
        mInfoBarObserver.waitUntilInfoBarDisappears();
    }

    @Test(expected = TimeoutException.class)
    @LargeTest
    public void testSyncErrorInfoBarNotShownWhenNoError() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setAuthError(GoogleServiceAuthError.State.NONE);
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(false);
        mFakeSyncServiceImpl.setRequiresClientUpgrade(false);

        @SyncError
        int syncError = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            mFakeSyncServiceImpl.setFirstSetupComplete(SyncFirstSetupCompleteSource.BASIC_FLOW);
            return SyncSettingsUtils.getSyncError();
        });

        Assert.assertEquals(
                SyncErrorPromptType.NOT_SHOWN, SyncErrorPromptUtils.getSyncErrorUiType(syncError));

        mInfoBarObserver.waitUntilInfoBarAppears(false);
    }

    @Test
    @LargeTest
    @DisabledTest(message = "https://crbug.com/1299060")
    public void testSyncErrorInfoBarIsNotShownBeforeMinimalIntervalPassed() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setAuthError(GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        mInfoBarObserver.waitUntilInfoBarAppears(false);

        // Close the SyncErrorInfoBar and reload the page again.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mSyncTestRule.getInfoBars().get(0).onCloseButtonClicked());
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        Assert.assertEquals(0, mSyncTestRule.getInfoBars().size());
        mInfoBarObserver.waitUntilInfoBarDisappears();

        // Override the time of last seen infobar to minimum required time before current time.
        SharedPreferencesManager.getInstance().writeLong(SYNC_ERROR_PROMPT_SHOWN_AT_TIME,
                System.currentTimeMillis() - SyncErrorPromptUtils.MINIMAL_DURATION_BETWEEN_UI_MS);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        mInfoBarObserver.waitUntilInfoBarAppears(true);
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorInfoBarForAuthErrorView() throws IOException {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setAuthError(GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        mRenderTestRule.render(mSyncTestRule.getInfoBarContainer().getContainerViewForTesting(),
                "sync_error_infobar_auth_error");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorInfoBarForSyncSetupIncompleteView() throws IOException {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        mRenderTestRule.render(mSyncTestRule.getInfoBarContainer().getContainerViewForTesting(),
                "sync_error_infobar_sync_setup_incomplete");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorInfoBarForPassphraseRequiredView() throws IOException {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        mRenderTestRule.render(mSyncTestRule.getInfoBarContainer().getContainerViewForTesting(),
                "sync_error_infobar_passphrase_required");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorInfoBarForClientOutOfDateView() throws IOException {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setRequiresClientUpgrade(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        mRenderTestRule.render(mSyncTestRule.getInfoBarContainer().getContainerViewForTesting(),
                "sync_error_infobar_client_out_of_date");
    }
}
