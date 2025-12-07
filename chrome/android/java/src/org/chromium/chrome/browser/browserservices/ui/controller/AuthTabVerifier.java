// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

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
import androidx.browser.auth.AuthTabIntent;
import androidx.browser.customtabs.CustomTabsService;

import org.chromium.base.CallbackController;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.Map;

/**
 * Runs Digital Asset Link verification for AuthTab, returns as Activity result for the matching
 * redirect URL when navigated to it.
 */
@NullMarked
public class AuthTabVerifier implements NativeInitObserver, DestroyObserver {
    private static boolean sDelayVerificationForTesting;

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CustomTabActivityTabProvider mTabProvider;
    private final @Nullable String mRedirectHost;
    private final @Nullable String mRedirectPath;

    private @Nullable ChromeOriginVerifier mOriginVerifier;

    /** Verification status. Updated for Android Asset Link API or Chrome verification process */
    private @VerificationStatus int mStatus;

    /** {@code True} if Android Asset Link API verification succeeded. */
    private boolean mVerifiedByAndroid;

    private @Nullable GURL mReturnUrl;
    private boolean mDestroyed;
    private int mActivityResult;
    private @Nullable Long mVerificationStartTime;
    private @Nullable Long mHttpsReturnAttemptTime;
    private @Nullable CallbackController mCallbackController;

    public AuthTabVerifier(
            Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabActivityTabProvider customTabActivityTabProvider) {
        mLifecycleDispatcher = lifecycleDispatcher;
        mIntentDataProvider = intentDataProvider;
        mTabProvider = customTabActivityTabProvider;
        mActivity = activity;
        mRedirectHost = mIntentDataProvider.getAuthRedirectHost();
        mRedirectPath = mIntentDataProvider.getAuthRedirectPath();
        mLifecycleDispatcher.register(this);

        mStatus = VerificationStatus.PENDING;
        mVerifiedByAndroid = false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            new AsyncTask<Boolean>() {
                @Override
                protected Boolean doInBackground() {
                    return isApprovedDomain(mRedirectHost);
                }

                @Override
                protected void onPostExecute(Boolean result) {
                    mVerifiedByAndroid = result;
                    if (result) mStatus = VerificationStatus.SUCCESS;
                }
            }.executeWithTaskTraits(TaskTraits.UI_DEFAULT);
        }
        mActivityResult = AuthTabIntent.RESULT_OK;
        maybeInitOriginVerifier();
    }

    @EnsuresNonNullIf("mOriginVerifier")
    private boolean maybeInitOriginVerifier() {
        if (!shouldRunOriginVerifier()) return false;

        if (mOriginVerifier == null) {
            WebContents webContents =
                    mTabProvider.getTab() != null ? mTabProvider.getTab().getWebContents() : null;
            mOriginVerifier =
                    ChromeOriginVerifierFactory.create(
                            mIntentDataProvider.getClientPackageName(),
                            CustomTabsService.RELATION_HANDLE_ALL_URLS,
                            webContents);
        }
        return true;
    }

    @VisibleForTesting
    boolean shouldRunOriginVerifier() {
        return !(mVerifiedByAndroid || mRedirectHost == null || mRedirectPath == null);
    }

    @Override
    public void onFinishNativeInitialization() {
        if (sDelayVerificationForTesting) return;

        if (!maybeInitOriginVerifier()) return;

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
                        mActivityResult = AuthTabIntent.RESULT_OK;
                    } else {
                        mStatus = VerificationStatus.FAILURE;
                        mActivityResult = AuthTabIntent.RESULT_VERIFICATION_FAILED;
                    }
                    // Handles the case where the DAL response comes after the user initiates login.
                    if (mReturnUrl != null) {
                        returnAsActivityResultInternal(mReturnUrl, /* customScheme= */ false);
                    }
                },
                assertNonNull(Origin.create(redirectUri)));
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
                && assumeNonNull(mRedirectHost).equals(url.getHost())
                && TextUtils.equals(mRedirectPath, url.getPath());
    }

    @RequiresApi(Build.VERSION_CODES.S)
    private boolean isApprovedDomain(@Nullable String host) {
        DomainVerificationManager manager =
                ContextUtils.getApplicationContext()
                        .getSystemService(DomainVerificationManager.class);

        DomainVerificationUserState userState = null;
        try {
            String packageName = mIntentDataProvider.getClientPackageName();
            assert packageName != null;
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
                    ChromeFeatureList.sCctAuthTabEnableHttpsRedirectsVerificationTimeoutMs
                            .getValue());
        }
    }

    private void returnAsActivityResultInternal(GURL url, boolean customScheme) {
        assert mStatus != VerificationStatus.PENDING : "Verification was not completed!";
        Intent intent = new Intent();

        int resultCode = customScheme ? AuthTabIntent.RESULT_OK : mActivityResult;
        if (resultCode == AuthTabIntent.RESULT_OK) {
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
        mActivityResult = AuthTabIntent.RESULT_VERIFICATION_TIMED_OUT;
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
