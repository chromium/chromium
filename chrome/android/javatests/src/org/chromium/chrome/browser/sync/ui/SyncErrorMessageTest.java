// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.browser.sync.ui.SyncErrorMessage.MessageType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.io.IOException;

/**
 * Test suites for {@link SyncErrorMessage}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(crbug.com/1168590): SyncTestRule doesn't support batching.")
@EnableFeatures({ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncErrorMessageTest {
    @Mock
    private MessageDispatcher mMessageDispatcher;
    private FakeSyncServiceImpl mFakeSyncServiceImpl;

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
        MockitoAnnotations.initMocks(this);
        SyncErrorMessageImpressionTracker.resetLastShownTime();
        mFakeSyncServiceImpl = (FakeSyncServiceImpl) mSyncTestRule.getSyncService();
        SyncErrorMessage.setMessageDispatcherForTesting(mMessageDispatcher);
        doAnswer((invocation) -> {
            PropertyModel model = invocation.getArgument(0);
            int dismissReason = invocation.getArgument(1);
            model.get(MessageBannerProperties.ON_DISMISSED).onResult(dismissReason);
            return null;
        })
                .when(mMessageDispatcher)
                .dismissMessage(any(), anyInt());
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageShownForAuthError() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setAuthError(GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasShownMessage();

        // Resolving the error should dismiss the current message.
        mFakeSyncServiceImpl.setAuthError(GoogleServiceAuthError.State.NONE);
        verifyHasDismissedMessage();
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageShownForSyncSetupIncomplete() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasShownMessage();

        // Resolving the error should dismiss the current message.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mFakeSyncServiceImpl.setFirstSetupComplete(SyncFirstSetupCompleteSource.BASIC_FLOW);
        });
        verifyHasDismissedMessage();
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageShownForPassphraseRequired() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasShownMessage();

        // Resolving the error should dismiss the current message.
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(false);
        verifyHasDismissedMessage();
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageShownForClientOutOfDate() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setRequiresClientUpgrade(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasShownMessage();

        // Not possible to resolve this error from within chrome unlike the other
        // SyncErrorMessage-s.
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageShownForTrustedVaultKeyRequired() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasShownMessage();

        // Resolving the error should dismiss the current message.
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(false);
        verifyHasDismissedMessage();
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageShownForTrustedVaultRecoverabilityDegraded() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultRecoverabilityDegraded(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasShownMessage();

        // Resolving the error should dismiss the current message.
        mFakeSyncServiceImpl.setTrustedVaultRecoverabilityDegraded(false);
        verifyHasDismissedMessage();
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageNotShownWhenNoError() throws Exception {
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

        Assert.assertEquals(MessageType.NOT_SHOWN, SyncErrorMessage.getMessageType(syncError));

        verifyHasNeverShownMessage();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES})
    public void testSyncErrorMessageForAuthErrorView() throws IOException {
        SyncErrorMessage.setMessageDispatcherForTesting(null);
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setAuthError(GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        ViewGroup view = mSyncTestRule.getActivity().findViewById(R.id.message_container);
        // Wait until the message ui is shown.
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(view.getChildCount(), Matchers.is(1)));
        mRenderTestRule.render(view, "sync_error_message_auth_error");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES})
    public void testSyncErrorMessageForAuthErrorViewModern() throws IOException {
        SyncErrorMessage.setMessageDispatcherForTesting(null);
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setAuthError(GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        ViewGroup view = mSyncTestRule.getActivity().findViewById(R.id.message_container);
        // Wait until the message ui is shown.
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(view.getChildCount(), Matchers.is(1)));
        mRenderTestRule.render(view, "sync_error_message_auth_error_modern");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorMessageForSyncSetupIncompleteView() throws IOException {
        SyncErrorMessage.setMessageDispatcherForTesting(null);
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        ViewGroup view = mSyncTestRule.getActivity().findViewById(R.id.message_container);
        // Wait until the message ui is shown.
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(view.getChildCount(), Matchers.is(1)));
        mRenderTestRule.render(view, "sync_error_message_sync_setup_incomplete");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorMessageForPassphraseRequiredView() throws IOException {
        SyncErrorMessage.setMessageDispatcherForTesting(null);
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        ViewGroup view = mSyncTestRule.getActivity().findViewById(R.id.message_container);
        // Wait until the message ui is shown.
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(view.getChildCount(), Matchers.is(1)));
        mRenderTestRule.render(view, "sync_error_message_passphrase_required");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorMessageForClientOutOfDateView() throws IOException {
        SyncErrorMessage.setMessageDispatcherForTesting(null);
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mFakeSyncServiceImpl.setRequiresClientUpgrade(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        ViewGroup view = mSyncTestRule.getActivity().findViewById(R.id.message_container);
        // Wait until the message ui is shown.
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(view.getChildCount(), Matchers.is(1)));
        mRenderTestRule.render(view, "sync_error_message_client_out_of_date");
    }

    private void verifyHasShownMessage() {
        verify(mMessageDispatcher,
                description("Message should be displayed when sync error occurs."))
                .enqueueWindowScopedMessage(any(), anyBoolean());
        Assert.assertNotNull(getSyncErrorMessage());
    }

    private void verifyHasNeverShownMessage() {
        verify(mMessageDispatcher,
                never().description(
                        "Message should be never displayed when sync error does not occur."))
                .enqueueWindowScopedMessage(any(), anyBoolean());
        Assert.assertNull(getSyncErrorMessage());
    }

    private void verifyHasDismissedMessage() {
        verify(mMessageDispatcher,
                description("Message should be dismissed when sync error has been resolved."))
                .dismissMessage(any(), anyInt());
        Assert.assertNull(getSyncErrorMessage());
    }

    private @Nullable SyncErrorMessage getSyncErrorMessage() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> SyncErrorMessage.getKeyForTesting().retrieveDataFromHost(
                                mSyncTestRule.getActivity()
                                        .getWindowAndroid()
                                        .getUnownedUserDataHost()));
    }
}
