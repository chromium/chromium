// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.PendingIntent;
import android.content.Intent;
import android.content.IntentSender;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.components.sync.TrustedVaultUserActionTriggerForUMA;

/**
 * {@link SyncTrustedVaultProxyActivity} has no own UI and just acts as a proxy to launch an
 * activity related to trusted vault user actions (passed via PendingIntent). The reason for using
 * this proxy activity is to detect when the proxied activity (key retrieval or degraded
 * recoverability fix UI) finishes and notify TrustedVaultClient about changes.
 */
public class SyncTrustedVaultProxyActivity extends AsyncInitializationActivity {
    private static final String TAG = "SyncUI";

    // Note that the implementation relies on request codes being >0 (default value for
    // |mRequestCode|).
    private static final int REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL = 1;
    private static final int REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED = 2;

    // Key names used when propagating extra param in the proxy intent.
    private static final String EXTRA_KEY_PROXIED_INTENT = "proxied_intent";
    private static final String EXTRA_KEY_REQUEST_CODE = "request_code";
    private static final String EXTRA_KEY_USER_ACTION_TRIGGER = "user_action_trigger";

    private @TrustedVaultUserActionTriggerForUMA int mUserActionTrigger;
    private int mRequestCode;

    /**
     * Creates an intent that launches an SyncTrustedVaultProxyActivity for the purpose of
     * key retrieval.
     *
     * @param keyRetrievalIntent Actual key retrieval intent, which will be launched by
     * SyncTrustedVaultProxyActivity.
     * @param userActionTrigger Enum representing which UI surface triggered the intent.
     *
     * @return the intent for launching SyncTrustedVaultProxyActivity
     */
    public static Intent createKeyRetrievalProxyIntent(
            PendingIntent keyRetrievalIntent,
            @TrustedVaultUserActionTriggerForUMA int userActionTrigger) {
        return createProxyIntent(
                keyRetrievalIntent, userActionTrigger, REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL);
    }

    /**
     * Creates an intent that launches an SyncTrustedVaultProxyActivity for the purpose of
     * fixing the recoverability degraded case.
     *
     * @param recoverabilityDegradedIntent Actual recoverability degraded fix intent, which will be
     *         launched by SyncTrustedVaultProxyActivity.
     * @param userActionTrigger Enum representing which UI surface triggered the intent.
     *
     * @return the intent for launching SyncTrustedVaultProxyActivity
     */
    public static Intent createRecoverabilityDegradedProxyIntent(
            PendingIntent recoverabilityDegradedIntent,
            @TrustedVaultUserActionTriggerForUMA int userActionTrigger) {
        return createProxyIntent(
                recoverabilityDegradedIntent,
                userActionTrigger,
                REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED);
    }

    private static Intent createProxyIntent(
            PendingIntent proxiedIntent,
            @TrustedVaultUserActionTriggerForUMA int userActionTrigger,
            int requestCode) {
        Intent proxyIntent =
                new Intent(
                        ContextUtils.getApplicationContext(), SyncTrustedVaultProxyActivity.class);
        proxyIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        proxyIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        proxyIntent.putExtra(EXTRA_KEY_PROXIED_INTENT, proxiedIntent);
        proxyIntent.putExtra(EXTRA_KEY_REQUEST_CODE, requestCode);
        proxyIntent.putExtra(EXTRA_KEY_USER_ACTION_TRIGGER, userActionTrigger);
        return proxyIntent;
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return false;
    }

    @Override
    protected void triggerLayoutInflation() {
        // This Activity has no own UI and uses external pending intent to provide it. Since this
        // Activity requires native initialization it implements AsyncInitializationActivity and
        // thus the pending intent is sent inside triggerLayoutInflation() instead of onCreate().
        PendingIntent proxiedIntent = getIntent().getParcelableExtra(EXTRA_KEY_PROXIED_INTENT);
        mRequestCode = getIntent().getIntExtra(EXTRA_KEY_REQUEST_CODE, -1);
        mUserActionTrigger = getIntent().getIntExtra(EXTRA_KEY_USER_ACTION_TRIGGER, -1);

        assert proxiedIntent != null;
        assert mRequestCode != -1;
        assert mUserActionTrigger != -1;

        try {
            startIntentSenderForResult(
                    proxiedIntent.getIntentSender(),
                    mRequestCode,
                    /* fillInIntent= */ null,
                    /* flagsMask= */ 0,
                    /* flagsValues= */ 0,
                    /* extraFlags= */ 0,
                    /* options= */ null);
        } catch (IntentSender.SendIntentException exception) {
            Log.w(TAG, "Error sending trusted vault intent: ", exception);
        }
        onInitialLayoutInflationComplete();
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        OneshotSupplierImpl<ProfileProvider> supplier = new OneshotSupplierImpl<>();
        ProfileProvider profileProvider =
                new ProfileProvider() {
                    @NonNull
                    @Override
                    public Profile getOriginalProfile() {
                        throw new IllegalStateException(
                                "Unexpected access of the original profile.");
                    }

                    @Nullable
                    @Override
                    public Profile getOffTheRecordProfile(boolean createIfNeeded) {
                        throw new IllegalStateException(
                                "Unexpected access of the incognito profile.");
                    }

                    @Override
                    public boolean hasOffTheRecordProfile() {
                        return false;
                    }
                };
        supplier.set(profileProvider);
        return supplier;
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        if (getSavedInstanceState() != null) {
            // The activity might be restored and this shouldn't cause recording the histogram
            // second time.
            return;
        }

        // Note that the metric-recording methods are invoked here, and not earlier, because:
        // a) The native part must be loaded (which is not guaranteed in triggerLayoutInflation).
        // b) It cannot be done too early, e.g. upon intent creation, because that doesn't always
        // mean the intent will actually be launched. This is particularly relevant for Android
        // notifications (SyncErrorNotifier), because the user may ignore or dismiss a notification.
        switch (mRequestCode) {
            case REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL:
                TrustedVaultClient.get().recordKeyRetrievalTrigger(mUserActionTrigger);
                break;

            case REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED:
                TrustedVaultClient.get().recordRecoverabilityDegradedFixTrigger(mUserActionTrigger);
                break;

            default:
                assert false;
        }
    }

    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent intent) {
        boolean result = super.onActivityResultWithNative(requestCode, resultCode, intent);

        switch (requestCode) {
            case REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL:
                // Upon key retrieval completion, the keys in TrustedVaultClient could have changed.
                // This is done even if the user cancelled the flow (i.e. resultCode != RESULT_OK)
                // because it's harmless to issue a redundant notifyKeysChanged().
                TrustedVaultClient.get().notifyKeysChanged();
                break;

            case REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED:
                // Same as above, it is harmless to issue redundant notifyRecoverabilityChanged().
                TrustedVaultClient.get().notifyRecoverabilityChanged();
                break;

            default:
                assert false;
        }

        finish();
        return result;
    }
}
