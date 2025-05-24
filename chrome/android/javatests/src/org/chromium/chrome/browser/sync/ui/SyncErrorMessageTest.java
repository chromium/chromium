// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;
import org.chromium.ui.modelutil.PropertyModel;

import java.io.IOException;

/** Test suites for {@link SyncErrorMessage}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(crbug.com/40743432): SyncTestRule doesn't support batching.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncErrorMessageTest {

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;
    @Mock private MessageDispatcher mMessageDispatcher;
    private FakeSyncServiceImpl mFakeSyncServiceImpl;
    private final Context mContext = ContextUtils.getApplicationContext();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final SyncTestRule mSyncTestRule =
            new SyncTestRule() {
                @Override
                protected FakeSyncServiceImpl createSyncServiceImpl() {
                    return new FakeSyncServiceImpl();
                }
            };

    private static final int RENDER_TEST_REVISION = 3;
    private static final String RENDER_TEST_DESCRIPTION = "Sync error message for identity errors.";

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setDescription(RENDER_TEST_DESCRIPTION)
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SYNC)
                    .build();

    @Before
    public void setUp() {
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);
        SyncErrorMessageImpressionTracker.resetLastShownTime();
        mFakeSyncServiceImpl = (FakeSyncServiceImpl) mSyncTestRule.getSyncService();
        SyncErrorMessage.setMessageDispatcherForTesting(mMessageDispatcher);
        doAnswer(
                        (invocation) -> {
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
    public void testSyncErrorMessageShownForAuthErrorForSignedInUsers() throws Exception {
        HistogramWatcher watchIdentityErrorMessageShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorMessage.AuthError",
                        SyncSettingsUtils.ErrorUiAction.SHOWN);

        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setAuthError(
                new GoogleServiceAuthError(GoogleServiceAuthErrorState.INVALID_GAIA_CREDENTIALS));
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasShownMessage();
        watchIdentityErrorMessageShownHistogram.assertExpected();

        // Resolving the error should dismiss the current message.
        mFakeSyncServiceImpl.setAuthError(
                new GoogleServiceAuthError(GoogleServiceAuthErrorState.NONE));
        verifyHasDismissedMessage();
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageShownForPassphraseRequiredForSignedInUsers() throws Exception {
        HistogramWatcher watchIdentityErrorMessageShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorMessage.PassphraseRequired",
                        SyncSettingsUtils.ErrorUiAction.SHOWN);

        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasShownMessage();
        watchIdentityErrorMessageShownHistogram.assertExpected();

        // Resolving the error should dismiss the current message.
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(false);
        verifyHasDismissedMessage();
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageShownForClientOutOfDateForSignedInUsers() throws Exception {
        HistogramWatcher watchIdentityErrorMessageShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorMessage.ClientOutOfDate",
                        SyncSettingsUtils.ErrorUiAction.SHOWN);

        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setRequiresClientUpgrade(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasShownMessage();
        watchIdentityErrorMessageShownHistogram.assertExpected();

        // Not possible to resolve this error from within chrome unlike the other
        // SyncErrorMessage-s.
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageShownForTrustedVaultKeyRequiredForSignedInUsers()
            throws Exception {
        HistogramWatcher watchIdentityErrorMessageShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorMessage.TrustedVaultKeyRequiredForPasswords",
                        SyncSettingsUtils.ErrorUiAction.SHOWN);

        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasShownMessage();
        watchIdentityErrorMessageShownHistogram.assertExpected();

        // Resolving the error should dismiss the current message.
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(false);
        verifyHasDismissedMessage();
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.SYNC_ENABLE_PASSWORDS_SYNC_ERROR_MESSAGE_ALTERNATIVE)
    public void testSyncErrorMessageForTrustedVaultKeyRequiredContent() throws Exception {
        ArgumentCaptor<PropertyModel> mModelCaptor = ArgumentCaptor.forClass(PropertyModel.class);

        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);

        verify(mMessageDispatcher).enqueueWindowScopedMessage(mModelCaptor.capture(), anyBoolean());
        PropertyModel mModel = mModelCaptor.getValue();
        Assert.assertEquals(
                mContext.getString(R.string.identity_error_card_button_verify),
                mModel.get(MessageBannerProperties.TITLE));
        Assert.assertEquals(
                mContext.getString(
                        R.string.identity_error_message_body_sync_retrieve_keys_for_passwords),
                mModel.get(MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals(
                mContext.getString(R.string.identity_error_message_button_verify),
                mModel.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
    }

    @Test
    @LargeTest
    @EnableFeatures(
            ChromeFeatureList.SYNC_ENABLE_PASSWORDS_SYNC_ERROR_MESSAGE_ALTERNATIVE + ":version/1")
    public void testSyncErrorMessageForTrustedVaultKeyRequiredContent_alternativeOne()
            throws Exception {
        ArgumentCaptor<PropertyModel> mModelCaptor = ArgumentCaptor.forClass(PropertyModel.class);

        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);

        verify(mMessageDispatcher).enqueueWindowScopedMessage(mModelCaptor.capture(), anyBoolean());
        PropertyModel mModel = mModelCaptor.getValue();
        Assert.assertEquals(
                mContext.getString(R.string.password_sync_trusted_vault_error_title),
                mModel.get(MessageBannerProperties.TITLE));
        Assert.assertEquals(
                mContext.getString(R.string.password_sync_trusted_vault_error_hint),
                mModel.get(MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals(
                mContext.getString(R.string.identity_error_message_button_verify),
                mModel.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
    }

    @Test
    @LargeTest
    @EnableFeatures(
            ChromeFeatureList.SYNC_ENABLE_PASSWORDS_SYNC_ERROR_MESSAGE_ALTERNATIVE + ":version/2")
    public void testSyncErrorMessageForTrustedVaultKeyRequiredContent_alternativeTwo()
            throws Exception {
        ArgumentCaptor<PropertyModel> mModelCaptor = ArgumentCaptor.forClass(PropertyModel.class);

        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);

        verify(mMessageDispatcher).enqueueWindowScopedMessage(mModelCaptor.capture(), anyBoolean());
        PropertyModel mModel = mModelCaptor.getValue();
        Assert.assertEquals(
                mContext.getString(R.string.password_sync_trusted_vault_error_title),
                mModel.get(MessageBannerProperties.TITLE));
        Assert.assertEquals(
                mContext.getString(R.string.password_sync_trusted_vault_error_hint),
                mModel.get(MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals(
                mContext.getString(R.string.identity_error_card_button_okay),
                mModel.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageShownForTrustedVaultRecoverabilityDegradedForSignedInUsers()
            throws Exception {
        HistogramWatcher watchIdentityErrorMessageShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorMessage.TrustedVaultRecoverabilityDegradedForPasswords",
                        SyncSettingsUtils.ErrorUiAction.SHOWN);

        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultRecoverabilityDegraded(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasShownMessage();
        watchIdentityErrorMessageShownHistogram.assertExpected();

        // Resolving the error should dismiss the current message.
        mFakeSyncServiceImpl.setTrustedVaultRecoverabilityDegraded(false);
        verifyHasDismissedMessage();
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageNotShownWhenNoErrorForSignedInUsers() throws Exception {
        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setAuthError(
                new GoogleServiceAuthError(GoogleServiceAuthErrorState.NONE));
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(false);
        mFakeSyncServiceImpl.setRequiresClientUpgrade(false);

        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasNeverShownMessage();
    }

    @Test
    @LargeTest
    public void testSyncErrorMessageNotShownForUpmBackendOutdatedSignedInUsers() {
        // Sign in.
        doReturn(true)
                .when(mPasswordManagerUtilBridgeJniMock)
                .isGmsCoreUpdateRequired(any(), any());
        mSyncTestRule.setUpAccountAndSignInForTesting();
        @SyncSettingsUtils.SyncError
        int syncError =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return SyncSettingsUtils.getIdentityError(
                                    mSyncTestRule.getProfile(/* incognito= */ false));
                        });
        Assert.assertEquals(SyncSettingsUtils.SyncError.UPM_BACKEND_OUTDATED, syncError);

        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        verifyHasNeverShownMessage();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorMessageForAuthErrorViewForSignedInUsers() throws IOException {
        SyncErrorMessage.setMessageDispatcherForTesting(null);
        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setAuthError(
                new GoogleServiceAuthError(GoogleServiceAuthErrorState.INVALID_GAIA_CREDENTIALS));
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        ViewGroup view = mSyncTestRule.getActivity().findViewById(R.id.message_container);
        // Wait until the message ui is shown.
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(view.getChildCount(), Matchers.is(1)));
        mRenderTestRule.render(view, "identity_error_message_auth_error");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorMessageForPassphraseRequiredViewForSignedInUsers() throws IOException {
        SyncErrorMessage.setMessageDispatcherForTesting(null);
        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        ViewGroup view = mSyncTestRule.getActivity().findViewById(R.id.message_container);
        // Wait until the message ui is shown.
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(view.getChildCount(), Matchers.is(1)));
        mRenderTestRule.render(view, "identity_error_message_passphrase_required");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorMessageForClientOutOfDateViewForSignedInUsers() throws IOException {
        SyncErrorMessage.setMessageDispatcherForTesting(null);
        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setRequiresClientUpgrade(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);
        ViewGroup view = mSyncTestRule.getActivity().findViewById(R.id.message_container);
        // Wait until the message ui is shown.
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(view.getChildCount(), Matchers.is(1)));
        mRenderTestRule.render(view, "identity_error_message_client_out_of_date");
    }

    @Test
    @LargeTest
    public void testActionForPassphraseRequiredForSignedInUsers() throws Exception {
        SyncErrorMessage.setMessageDispatcherForTesting(null);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Sync.IdentityErrorMessage.PassphraseRequired",
                                SyncSettingsUtils.ErrorUiAction.SHOWN)
                        .expectIntRecord(
                                "Sync.IdentityErrorMessage.PassphraseRequired",
                                SyncSettingsUtils.ErrorUiAction.BUTTON_CLICKED)
                        .build();

        // Sign in.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrl(UrlConstants.VERSION_URL);

        Intents.init();
        onViewWaiting(allOf(withText("Enter"), isDisplayed())).perform(click());
        intended(
                IntentMatchers.hasExtra(
                        SettingsActivity.EXTRA_SHOW_FRAGMENT, ManageSyncSettings.class.getName()));
        Intents.release();

        histogramWatcher.assertExpected();
    }

    private void verifyHasShownMessage() {
        verify(
                        mMessageDispatcher,
                        description("Message should be displayed when sync error occurs."))
                .enqueueWindowScopedMessage(any(), anyBoolean());
        Assert.assertNotNull(getSyncErrorMessage());
    }

    private void verifyHasNeverShownMessage() {
        verify(
                        mMessageDispatcher,
                        never().description(
                                        "Message should be never displayed when sync error does not"
                                                + " occur."))
                .enqueueWindowScopedMessage(any(), anyBoolean());
        Assert.assertNull(getSyncErrorMessage());
    }

    private void verifyHasDismissedMessage() {
        verify(
                        mMessageDispatcher,
                        description(
                                "Message should be dismissed when sync error has been resolved."))
                .dismissMessage(any(), anyInt());
        Assert.assertNull(getSyncErrorMessage());
    }

    private @Nullable SyncErrorMessage getSyncErrorMessage() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        SyncErrorMessage.getKeyForTesting()
                                .retrieveDataFromHost(
                                        mSyncTestRule
                                                .getActivity()
                                                .getWindowAndroid()
                                                .getUnownedUserDataHost()));
    }
}
