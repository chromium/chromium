// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.trusted_vault;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.PendingIntent;
import android.content.Intent;
import android.content.IntentSender;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.components.trusted_vault.SecurityDomainId;
import org.chromium.components.trusted_vault.TrustedVaultClient;
import org.chromium.components.trusted_vault.TrustedVaultClient.ProxyIntentFactory;
import org.chromium.components.trusted_vault.TrustedVaultUserActionTriggerForUMA;

/**
 * {@link TrustedVaultProxyActivity} has no own UI and just acts as a proxy to launch an activity
 * related to trusted vault user actions (passed via PendingIntent). The reason for using this proxy
 * activity is to detect when the proxied activity (key retrieval or degraded recoverability fix UI)
 * finishes and notify TrustedVaultClient about changes.
 */
@NullMarked
public class TrustedVaultProxyActivity extends AsyncInitializationActivity {
    /** {@link ServiceImpl} implementation of {@link ProxyIntentFactory}. */
    @ServiceImpl(ProxyIntentFactory.class)
    public static final class ProxyIntentFactoryImpl implements ProxyIntentFactory {
        /** {@link ProxyIntentFactory} implementation. */
        @Override
        public Intent createKeyRetrievalProxyIntent(
                PendingIntent keyRetrievalIntent,
                @TrustedVaultUserActionTriggerForUMA int userActionTrigger,
                @SecurityDomainId int securityDomainId) {
            return createProxyIntent(
                    keyRetrievalIntent,
                    userActionTrigger,
                    REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL,
                    securityDomainId);
        }

        /** {@link ProxyIntentFactory} implementation. */
        @Override
        public Intent createRecoverabilityDegradedProxyIntent(
                PendingIntent recoverabilityDegradedIntent,
                @TrustedVaultUserActionTriggerForUMA int userActionTrigger,
                @SecurityDomainId int securityDomainId) {
            return createProxyIntent(
                    recoverabilityDegradedIntent,
                    userActionTrigger,
                    REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED,
                    securityDomainId);
        }
    }

    private static final String TAG = "SyncUI";

    // Note that the implementation relies on request codes being >0 (default value for
    // |mRequestCode|).
    private static final int REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL = 1;
    private static final int REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED = 2;

    // Key names used when propagating extra param in the proxy intent.
    private static final String EXTRA_KEY_PROXIED_INTENT = "proxied_intent";
    private static final String EXTRA_KEY_REQUEST_CODE = "request_code";
    private static final String EXTRA_KEY_USER_ACTION_TRIGGER = "user_action_trigger";
    private static final String EXTRA_KEY_SECURITY_DOMAIN_ID = "security_domain_id";

    private @TrustedVaultUserActionTriggerForUMA int mUserActionTrigger;
    private int mRequestCode;
    private @SecurityDomainId int mSecurityDomainId;

    private static Intent createProxyIntent(
            PendingIntent proxiedIntent,
            @TrustedVaultUserActionTriggerForUMA int userActionTrigger,
            int requestCode,
            @SecurityDomainId int securityDomainId) {
        Intent proxyIntent =
                new Intent(ContextUtils.getApplicationContext(), TrustedVaultProxyActivity.class);
        proxyIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        proxyIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        proxyIntent.putExtra(EXTRA_KEY_PROXIED_INTENT, proxiedIntent);
        proxyIntent.putExtra(EXTRA_KEY_REQUEST_CODE, requestCode);
        proxyIntent.putExtra(EXTRA_KEY_USER_ACTION_TRIGGER, userActionTrigger);
        proxyIntent.putExtra(EXTRA_KEY_SECURITY_DOMAIN_ID, securityDomainId);
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
        PendingIntent proxiedIntent =
                IntentUtils.safeGetParcelableExtra(getIntent(), EXTRA_KEY_PROXIED_INTENT);
        mRequestCode = getIntent().getIntExtra(EXTRA_KEY_REQUEST_CODE, -1);
        mUserActionTrigger = getIntent().getIntExtra(EXTRA_KEY_USER_ACTION_TRIGGER, -1);
        mSecurityDomainId = getIntent().getIntExtra(EXTRA_KEY_SECURITY_DOMAIN_ID, -1);

        assert proxiedIntent != null;
        assert mRequestCode != -1;
        assert mUserActionTrigger != -1;
        assert mSecurityDomainId != -1;

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

                    @Override
                    public Profile getOriginalProfile() {
                        assert false;
                        return assumeNonNull(null);
                    }

                    @Override
                    public @Nullable Profile getOffTheRecordProfile(boolean createIfNeeded) {
                        assert !createIfNeeded;
                        return null;
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
                TrustedVaultClient.get(mSecurityDomainId)
                        .recordKeyRetrievalTrigger(mUserActionTrigger);
                break;

            case REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED:
                TrustedVaultClient.get(mSecurityDomainId)
                        .recordRecoverabilityDegradedFixTrigger(mUserActionTrigger);
                break;

            default:
                assert false;
        }
    }

    @Override
    public boolean onActivityResultWithNative(
            int requestCode, int resultCode, @Nullable Intent intent) {
        boolean result = super.onActivityResultWithNative(requestCode, resultCode, intent);

        switch (requestCode) {
            case REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL:
                // Upon key retrieval completion, the keys in TrustedVaultClient could have changed.
                // This is done even if the user cancelled the flow (i.e. resultCode != RESULT_OK)
                // because it's harmless to issue a redundant notifyKeysChanged().
                TrustedVaultClient.get(mSecurityDomainId).notifyKeysChanged(mUserActionTrigger);
                break;

            case REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED:
                // Same as above, it is harmless to issue redundant notifyRecoverabilityChanged().
                TrustedVaultClient.get(mSecurityDomainId).notifyRecoverabilityChanged();
                break;

            default:
                assert false;
        }

        finish();
        return result;
    }
}
