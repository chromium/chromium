// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.verify.domain.DomainVerificationManager;
import android.content.pm.verify.domain.DomainVerificationUserState;
import android.net.Uri;
import android.os.Build;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsService;

import org.chromium.base.CallbackController;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.Map;

import javax.inject.Inject;

/**
 * Runs Digital Asset Link verification for AuthTab, returns as Activity result for the matching
 * redirect URL when navigated to it.
 */
@ActivityScope
public class AuthTabVerifier implements NativeInitObserver, DestroyObserver {
    // Value to return as activity result when the verification failed.
    // TODO(358167556): Move this to AndroidX.
    public static final int RESULT_VERIFICATION_FAILED = 2;
    public static final int RESULT_VERIFICATION_TIMED_OUT = 3;

    @VisibleForTesting static final long VERIFICATION_TIMEOUT_MS = 10000;

    private static boolean sDelayVerificationForTesting;

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final String mRedirectHost;
    private final String mRedirectPath;

    private ChromeOriginVerifier mOriginVerifier;

    /** Verification status. Updated for Android Asset Link API or Chrome verification process */
    private @VerificationStatus int mStatus;

    /** {@code True} if Android Asset Link API verification succeeded. */
    private boolean mVerifiedByAndroid;

    private GURL mReturnUrl;
    private boolean mDestroyed;
    private int mActivityResult;
    private Long mVerificationStartTime;
    private Long mHttpsReturnAttemptTime;
    private CallbackController mCallbackController;

    @Inject
    public AuthTabVerifier(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CustomTabActivityTabProvider tabProvider,
            BrowserServicesIntentDataProvider intentDataProvider,
            ChromeOriginVerifierFactory originVerifierFactory,
            Activity activity,
            ExternalAuthUtils externalAuthUtils) {
        mLifecycleDispatcher = lifecycleDispatcher;
        mIntentDataProvider = intentDataProvider;
        mActivity = activity;
        mRedirectHost = intentDataProvider.getAuthRedirectHost();
        mRedirectPath = intentDataProvider.getAuthRedirectPath();
        mLifecycleDispatcher.register(this);

        // TODO(b/358167556): Do this in a background to avoid potential ANR from system IPC call.
        mVerifiedByAndroid =
                android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S
                        && isApprovedDomain(mRedirectHost);
        mStatus = mVerifiedByAndroid ? VerificationStatus.SUCCESS : VerificationStatus.PENDING;
        mActivityResult = Activity.RESULT_OK;

        if (shouldRunOriginVerifier()) {
            WebContents webContents =
                    tabProvider.getTab() != null ? tabProvider.getTab().getWebContents() : null;
            mOriginVerifier =
                    originVerifierFactory.create(
                            intentDataProvider.getClientPackageName(),
                            CustomTabsService.RELATION_HANDLE_ALL_URLS,
                            webContents,
                            externalAuthUtils);
        }
    }

    private boolean shouldRunOriginVerifier() {
        return !(mVerifiedByAndroid || mRedirectHost == null || mRedirectPath == null);
    }

    @Override
    public void onFinishNativeInitialization() {
        if (!shouldRunOriginVerifier()) return;

        if (sDelayVerificationForTesting) return;

        // Start verification against the redirect URL
        Uri redirectUri =
                new Uri.Builder()
                        .scheme(UrlConstants.HTTPS_SCHEME)
                        .authority(mRedirectHost)
                        .path(mRedirectPath)
                        .build();
        mVerificationStartTime = SystemClock.elapsedRealtime();
        mOriginVerifier.start(
                (packageName, unused, verified, online) -> {
                    if (mDestroyed) return;
                    if (verified) {
                        mStatus = VerificationStatus.SUCCESS;
                        mActivityResult = Activity.RESULT_OK;
                    } else {
                        mStatus = VerificationStatus.FAILURE;
                        mActivityResult = RESULT_VERIFICATION_FAILED;
                    }
                    // Handles the case where the DAL response comes after the user initiates login.
                    if (mReturnUrl != null) {
                        returnAsActivityResultInternal(mReturnUrl, /* customScheme= */ false);
                    }
                },
                Origin.create(redirectUri));
    }

    /**
     * Checks whether we should return URL as Activity result for the matched redirect URL. 1)
     * Android Asset Link API already finished in success 2) Chrome's own verification succeeded
     *
     * <p>Verification still in pending state is also regarded as the case where returning URL
     * should happen, but it is delayed till the verification is completed.
     *
     * @param url URL being navigated to
     */
    public boolean shouldRedirectHttpsAuthUrl(GURL url) {
        if (!isRedirectUrl(url)) return false;

        if (mStatus == VerificationStatus.PENDING) mReturnUrl = url;
        return true;
    }

    private boolean isRedirectUrl(GURL url) {
        return UrlConstants.HTTPS_SCHEME.equals(url.getScheme())
                && mRedirectHost.equals(url.getHost())
                && TextUtils.equals(mRedirectPath, url.getPath());
    }

    @RequiresApi(Build.VERSION_CODES.S)
    private boolean isApprovedDomain(String host) {
        DomainVerificationManager manager =
                ContextUtils.getApplicationContext()
                        .getSystemService(DomainVerificationManager.class);

        DomainVerificationUserState userState = null;
        try {
            String packageName = mIntentDataProvider.getClientPackageName();
            userState = manager.getDomainVerificationUserState(packageName);
        } catch (PackageManager.NameNotFoundException e) {
            // fall through
        }
        if (userState == null) return false;
        Map<String, Integer> hostToStateMap = userState.getHostToStateMap();
        for (String domain : hostToStateMap.keySet()) {
            Integer stateValue = hostToStateMap.get(domain);
            if (stateValue == DomainVerificationUserState.DOMAIN_STATE_VERIFIED
                    && TextUtils.equals(host, domain)) {
                return true;
            }
        }
        return false;
    }

    boolean hasValidatedHttps() {
        return mVerifiedByAndroid || mStatus != VerificationStatus.PENDING;
    }

    public boolean isCustomScheme(GURL url) {
        String redirectScheme = mIntentDataProvider.getAuthRedirectScheme();
        return !TextUtils.isEmpty(redirectScheme)
                && !UrlUtilities.isAcceptedScheme(url)
                && url.getScheme().equals(redirectScheme);
    }

    /** Return the given URL as activity result. */
    public void returnAsActivityResult(GURL url) {
        // Return results only if https redirection URL verification got completed or the URL is of
        // a custom scheme, whichever comes first.
        // TODO(358167556): Consider allowing only one of the two depending on how the auth tab
        //     intent was configured. Currently we do not expect both custom scheme/https host+path
        //     to be set.
        boolean customScheme = isCustomScheme(url);
        if (hasValidatedHttps() || customScheme) {
            returnAsActivityResultInternal(url, customScheme);
        } else {
            mHttpsReturnAttemptTime = SystemClock.elapsedRealtime();
            mCallbackController = new CallbackController();
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    mCallbackController.makeCancelable(this::returnTimeoutAsActivityResult),
                    VERIFICATION_TIMEOUT_MS);
        }
    }

    private void returnAsActivityResultInternal(GURL url, boolean customScheme) {
        assert mStatus != VerificationStatus.PENDING : "Verification was not completed!";
        Intent intent = new Intent();

        int resultCode = customScheme ? Activity.RESULT_OK : mActivityResult;
        if (resultCode == Activity.RESULT_OK) {
            intent.setData(Uri.parse(url.getSpec()));
        }

        if (mVerificationStartTime != null) {
            long elapsedSinceVerificationStart =
                    SystemClock.elapsedRealtime() - mVerificationStartTime;
            RecordHistogram.recordTimesHistogram(
                    "CustomTabs.AuthTab.TimeToDalVerification.SinceStart",
                    elapsedSinceVerificationStart);
            mVerificationStartTime = null;
        }

        if (mHttpsReturnAttemptTime != null) {
            long elapsedSinceReturnAttempt =
                    SystemClock.elapsedRealtime() - mHttpsReturnAttemptTime;
            RecordHistogram.recordTimesHistogram(
                    "CustomTabs.AuthTab.TimeToDalVerification.SinceFlowCompletion",
                    elapsedSinceReturnAttempt);
            mHttpsReturnAttemptTime = null;
        }

        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

        // Canceling/user-initiated closing of custom-scheme AuthTab flow doesn't end here.
        mActivity.setResult(resultCode, intent);
        mActivity.finish();
        mReturnUrl = null;
    }

    private void returnTimeoutAsActivityResult() {
        mStatus = VerificationStatus.FAILURE;
        mActivityResult = RESULT_VERIFICATION_TIMED_OUT;
        returnAsActivityResultInternal(GURL.emptyGURL(), false);
    }

    @Override
    public void onDestroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        mVerificationStartTime = null;
        mHttpsReturnAttemptTime = null;
        mDestroyed = true;
        mLifecycleDispatcher.unregister(this);
    }

    public static void setDelayVerificationForTesting(boolean delay) {
        sDelayVerificationForTesting = delay;
    }
}
