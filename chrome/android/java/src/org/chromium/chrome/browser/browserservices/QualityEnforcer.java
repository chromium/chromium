// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.ClientPackageNameProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.net.NetError;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

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
public class QualityEnforcer implements NativeInitObserver {
    @VisibleForTesting
    static final String CRASH = "quality_enforcement.crash";
    @VisibleForTesting
    static final String KEY_CRASH_REASON = "crash_reason";
    @VisibleForTesting
    static final String KEY_SUCCESS = "success";

    private final ChromeActivity<?> mActivity;
    private final Verifier mVerifier;
    private final CustomTabsConnection mConnection;
    private final CustomTabsSessionToken mSessionToken;
    private final ClientPackageNameProvider mClientPackageNameProvider;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final TrustedWebActivityUmaRecorder mUmaRecorder;

    private boolean mOriginVerified;

    // Do not modify or reuse existing entries, they are used in a UMA histogram. Please also edit
    // TrustedWebActivityQualityEnforcementViolationType in enums.xml if new value added.
    @IntDef({ViolationType.ERROR_404, ViolationType.ERROR_5XX, ViolationType.UNAVAILABLE_OFFLINE,
            ViolationType.DIGITAL_ASSETLINKS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViolationType {
        int ERROR_404 = 0;
        int ERROR_5XX = 1;
        int UNAVAILABLE_OFFLINE = 2;
        int DIGITAL_ASSETLINKS = 3;
        int NUM_ENTRIES = 4;
    }

    private final CustomTabTabObserver mTabObserver = new CustomTabTabObserver() {
        @Override
        public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
            if (!navigation.hasCommitted() || !navigation.isInMainFrame()
                    || navigation.isSameDocument()) {
                return;
            }

            String newUrl = tab.getOriginalUrl();
            if (isNavigationInScope(newUrl)) {
                if (navigation.httpStatusCode() == 404) {
                    trigger(ViolationType.ERROR_404, newUrl, navigation.httpStatusCode());
                } else if (navigation.httpStatusCode() >= 500
                        && navigation.httpStatusCode() <= 599) {
                    trigger(ViolationType.ERROR_5XX, newUrl, navigation.httpStatusCode());
                } else if (navigation.errorCode() == NetError.ERR_INTERNET_DISCONNECTED) {
                    trigger(ViolationType.UNAVAILABLE_OFFLINE, newUrl, navigation.httpStatusCode());
                }
            }
        }

        @Override
        public void onObservingDifferentTab(@NonNull Tab tab) {
            // On tab switches, update the stored verification state.
            isNavigationInScope(tab.getOriginalUrl());
        }
    };

    @Inject
    public QualityEnforcer(ChromeActivity<?> activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
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
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onFinishNativeInitialization() {
        String url = mIntentDataProvider.getUrlToLoad();
        mVerifier.verify(url).then((verified) -> {
            if (!verified) {
                trigger(ViolationType.DIGITAL_ASSETLINKS, mIntentDataProvider.getUrlToLoad(), 0);
            }
        });
    }

    private void trigger(@ViolationType int type, String url, int httpStatusCode) {
        mUmaRecorder.recordQualityEnforcementViolation(type, false /* crashed */);

        if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT)) {
            showErrorToast(getToastMessage(type, url, httpStatusCode));
            return;
        }

        // Notify the client app.
        Bundle args = new Bundle();
        args.putString(KEY_CRASH_REASON, toTwaCrashMessage(type, url, httpStatusCode));
        Bundle result = mConnection.sendExtraCallbackWithResult(mSessionToken, CRASH, args);
        boolean success = result != null && result.getBoolean(KEY_SUCCESS);

        // Show the Toast if client app does not enable quality enforcement.
        if (!success) {
            showErrorToast(getToastMessage(type, url, httpStatusCode));
        }

        // Do not crash on assetlink failures if the client app does not have installer package
        // name.
        if (type == ViolationType.DIGITAL_ASSETLINKS && !isDebugInstall()) {
            return;
        }

        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT_FORCED)
                || success) {
            mUmaRecorder.recordQualityEnforcementViolation(type, true /* crashed */);
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
    private boolean isNavigationInScope(String newUrl) {
        if (newUrl.equals("")) return false;
        boolean wasVerified = mOriginVerified;
        Promise<Boolean> result = mVerifier.verify(newUrl);
        mOriginVerified = !result.isFulfilled() || result.getResult();
        return wasVerified && mOriginVerified;
    }

    /* Get the localized string for toast message. */
    private String getToastMessage(@ViolationType int type, String url, int httpStatusCode) {
        switch (type) {
            case ViolationType.ERROR_404:
            case ViolationType.ERROR_5XX:
                return ContextUtils.getApplicationContext().getString(
                        R.string.twa_quality_enforcement_violation_error, httpStatusCode, url);
            case ViolationType.UNAVAILABLE_OFFLINE:
                return ContextUtils.getApplicationContext().getString(
                        R.string.twa_quality_enforcement_violation_offline, url);
            case ViolationType.DIGITAL_ASSETLINKS:
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
    private String toTwaCrashMessage(@ViolationType int type, String url, int httpStatusCode) {
        switch (type) {
            case ViolationType.ERROR_404:
            case ViolationType.ERROR_5XX:
                return httpStatusCode + " on " + url;
            case ViolationType.UNAVAILABLE_OFFLINE:
                return "Page unavailable offline: " + url;
            case ViolationType.DIGITAL_ASSETLINKS:
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
}
