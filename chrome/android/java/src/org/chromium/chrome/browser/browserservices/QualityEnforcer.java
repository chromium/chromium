// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.Promise;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.constants.QualityEnforcementViolationType;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.ClientPackageNameProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.net.NetError;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.util.List;

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
    private final CustomTabsConnection mConnection;
    private final CustomTabsSessionToken mSessionToken;
    private final ClientPackageNameProvider mClientPackageNameProvider;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final TrustedWebActivityUmaRecorder mUmaRecorder;

    private boolean mFirstNavigationFinished;
    private boolean mOriginVerified;

    private final CustomTabTabObserver mTabObserver = new CustomTabTabObserver() {
        @Override
        public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
            if (!navigation.hasCommitted() || navigation.isSameDocument()) {
                return;
            }

            if (!mFirstNavigationFinished) {
                String loadUrl = mIntentDataProvider.getUrlToLoad();
                mFirstNavigationFinished = true;
                mVerifier.verify(loadUrl).then((verified) -> {
                    if (!verified) {
                        trigger(tab, QualityEnforcementViolationType.DIGITAL_ASSET_LINK,
                                mIntentDataProvider.getUrlToLoad(), 0);
                    }
                });
            }

            GURL newUrl = tab.getOriginalUrl();
            if (isNavigationInScope(newUrl)) {
                if (navigation.httpStatusCode() == 404) {
                    trigger(tab, QualityEnforcementViolationType.HTTP_ERROR404, newUrl.getSpec(),
                            navigation.httpStatusCode());
                } else if (navigation.httpStatusCode() >= 500
                        && navigation.httpStatusCode() <= 599) {
                    trigger(tab, QualityEnforcementViolationType.HTTP_ERROR5XX, newUrl.getSpec(),
                            navigation.httpStatusCode());
                } else if (navigation.errorCode() == NetError.ERR_INTERNET_DISCONNECTED) {
                    trigger(tab, QualityEnforcementViolationType.UNAVAILABLE_OFFLINE,
                            newUrl.getSpec(), navigation.httpStatusCode());
                }
            }
        }

        @Override
        public void onDidFinishNavigationNoop(Tab tab, NavigationHandle navigation) {
            if (!navigation.isInPrimaryMainFrame()) return;
        }

        @Override
        public void onObservingDifferentTab(@NonNull Tab tab) {
            // On tab switches, update the stored verification state.
            isNavigationInScope(tab.getOriginalUrl());
        }
    };

    @Inject
    public QualityEnforcer(Activity activity, ActivityLifecycleDispatcher lifecycleDispatcher,
            TabObserverRegistrar tabObserverRegistrar,
            BrowserServicesIntentDataProvider intentDataProvider, CustomTabsConnection connection,
            Verifier verifier, ClientPackageNameProvider clientPackageNameProvider,
            TrustedWebActivityUmaRecorder umaRecorder) {
        mActivity = activity;
        mVerifier = verifier;
        mSessionToken = intentDataProvider.getSession();
        mIntentDataProvider = intentDataProvider;
        mConnection = connection;
        mClientPackageNameProvider = clientPackageNameProvider;
        mUmaRecorder = umaRecorder;
        // Initialize the value to true before the first navigation.
        mOriginVerified = true;
        tabObserverRegistrar.registerActivityTabObserver(mTabObserver);
    }

    private void trigger(
            Tab tab, @QualityEnforcementViolationType int type, String url, int httpStatusCode) {
        mUmaRecorder.recordQualityEnforcementViolation(tab.getWebContents(), type);

        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT_WARNING)) {
            showErrorToast(getToastMessage(type, url, httpStatusCode));

            if (tab.getWebContents() != null) {
                String packageName = null;
                String signature = null;
                // Only get the package name and signature when violation type is
                // DIGITAL_ASSET_LINK. This is because computing the fingerprint is expensive.
                // We should figure out how to reuse the existing one in OriginVerifier.
                if (type == QualityEnforcementViolationType.DIGITAL_ASSET_LINK) {
                    packageName = mClientPackageNameProvider.get();
                    PackageManager pm = mActivity.getPackageManager();
                    List<String> signatures =
                            PackageUtils.getCertificateSHA256FingerprintForPackage(packageName);

                    // Sometimes information about the current package cannot be found - perhaps
                    // the user uninstalled the TWA while using it? See https://crbug.com/1358864.
                    if (signatures != null && signatures.size() > 0) {
                        signature = signatures.get(0);
                    }
                }

                QualityEnforcerJni.get().reportDevtoolsIssue(tab.getWebContents().getMainFrame(),
                        type, url, httpStatusCode, packageName, signature);
            }
        }

        if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT)) {
            return;
        }

        // Notify the client app.
        Bundle args = new Bundle();
        args.putString(KEY_CRASH_REASON, toTwaCrashMessage(type, url, httpStatusCode));
        Bundle result = mConnection.sendExtraCallbackWithResult(mSessionToken, CRASH, args);
        boolean success = result != null && result.getBoolean(KEY_SUCCESS);

        // Do not crash on assetlink failures if the client app does not have installer package
        // name.
        if (type == QualityEnforcementViolationType.DIGITAL_ASSET_LINK && !isDebugInstall()) {
            return;
        }

        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT_FORCED)
                || success) {
            mUmaRecorder.recordQualityEnforcementViolationCrashed(type);
            mActivity.finish();
        }
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
