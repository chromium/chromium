// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.constants.QualityEnforcementViolationType;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.ClientPackageNameProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import javax.inject.Inject;

/**
 * This class enforces a quality bar on the websites shown inside Trusted Web Activities. For
 * example triggering if a link from a page on the verified origin 404s.
 *
 * The current plan for when QualityEnforcer is triggered is to finish Chrome and send a signal
 * back to the TWA shell, causing it to crash. The purpose of this is to bring TWAs in line with
 * native applications - if a native application tries to start an Activity that doesn't exist, it
 * will crash. We should hold web apps to the same standard.
 */
@ActivityScope
public class QualityEnforcer {
    @VisibleForTesting
    static final String CRASH = "quality_enforcement.crash";
    @VisibleForTesting
    static final String KEY_CRASH_REASON = "crash_reason";
    @VisibleForTesting
    static final String KEY_SUCCESS = "success";

    private final Activity mActivity;
    private final Verifier mVerifier;
    private final CustomTabsSessionToken mSessionToken;
    private final ClientPackageNameProvider mClientPackageNameProvider;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final TrustedWebActivityUmaRecorder mUmaRecorder;

    private boolean mFirstNavigationFinished;
    private boolean mOriginVerified;

    @Inject
    public QualityEnforcer(Activity activity, ActivityLifecycleDispatcher lifecycleDispatcher,
            BrowserServicesIntentDataProvider intentDataProvider,
            Verifier verifier, ClientPackageNameProvider clientPackageNameProvider,
            TrustedWebActivityUmaRecorder umaRecorder) {
        mActivity = activity;
        mVerifier = verifier;
        mSessionToken = intentDataProvider.getSession();
        mIntentDataProvider = intentDataProvider;
        mClientPackageNameProvider = clientPackageNameProvider;
        mUmaRecorder = umaRecorder;
        // Initialize the value to true before the first navigation.
        mOriginVerified = true;
    }

    private void trigger(
            Tab tab, @QualityEnforcementViolationType int type, String url, int httpStatusCode) {
    }

    private void showErrorToast(String message) {
        Context context = ContextUtils.getApplicationContext();
        PackageManager pm = context.getPackageManager();
        // Only shows the toast when the TWA client app does not have installer info, i.e. install
        // via adb instead of a store.
        if (!isDebugInstall()) {
            Toast.makeText(context, message, Toast.LENGTH_LONG).show();
        }
    }

    /*
     * Updates whether the current url is verified and returns whether the source and destination
     * are both on the verified origin.
     */
    private boolean isNavigationInScope(GURL newUrl) {
        if (newUrl.isEmpty()) return false;
        boolean wasVerified = mOriginVerified;
        // TODO(crbug/783819): Migrate Verifier to GURL.
        Promise<Boolean> result = mVerifier.verify(newUrl.getSpec());
        mOriginVerified = !result.isFulfilled() || result.getResult();
        return wasVerified && mOriginVerified;
    }

    /* Get the localized string for toast message. */
    private String getToastMessage(
            @QualityEnforcementViolationType int type, String url, int httpStatusCode) {
        switch (type) {
            case QualityEnforcementViolationType.HTTP_ERROR404:
            case QualityEnforcementViolationType.HTTP_ERROR5XX:
                return ContextUtils.getApplicationContext().getString(
                        R.string.twa_quality_enforcement_violation_error, httpStatusCode, url);
            case QualityEnforcementViolationType.UNAVAILABLE_OFFLINE:
                return ContextUtils.getApplicationContext().getString(
                        R.string.twa_quality_enforcement_violation_offline, url);
            case QualityEnforcementViolationType.DIGITAL_ASSET_LINK:
                return ContextUtils.getApplicationContext().getString(
                        R.string.twa_quality_enforcement_violation_asset_link, url);
            default:
                return "";
        }
    }

    /*
     * Get the string for sending message to TWA client app. We are not using the localized one as
     * the toast because this is used in TWA's crash message.
     */
    private String toTwaCrashMessage(
            @QualityEnforcementViolationType int type, String url, int httpStatusCode) {
        switch (type) {
            case QualityEnforcementViolationType.HTTP_ERROR404:
            case QualityEnforcementViolationType.HTTP_ERROR5XX:
                return httpStatusCode + " on " + url;
            case QualityEnforcementViolationType.UNAVAILABLE_OFFLINE:
                return "Page unavailable offline: " + url;
            case QualityEnforcementViolationType.DIGITAL_ASSET_LINK:
                return "Digital asset links verification failed on " + url;
            default:
                return "";
        }
    }

    private boolean isDebugInstall() {
        // TODO(crbug.com/1136153) Need to figure out why the client package name can be null.
        if (mClientPackageNameProvider.get() == null) return false;

        return ContextUtils.getApplicationContext().getPackageManager().getInstallerPackageName(
                       mClientPackageNameProvider.get())
                != null;
    }

    @NativeMethods
    interface Natives {
        void reportDevtoolsIssue(RenderFrameHost renderFrameHost, int type, String url,
                int httpStatusCode, String packageName, String signature);
    }
}
