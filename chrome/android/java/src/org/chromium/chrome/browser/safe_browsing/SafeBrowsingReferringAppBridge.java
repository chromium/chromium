// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import android.app.Activity;
import android.content.Intent;
import android.provider.Browser;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.ExternalAppId;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.ui.base.WindowAndroid;

/** Bridge between Java and native SafeBrowsing code to get referring app information. */
public class SafeBrowsingReferringAppBridge {
    private SafeBrowsingReferringAppBridge() {}

    /** A helper class to store referring app information. */
    static class ReferringAppInfo {
        // The source of referring app name. These values must be aligned with the
        // ReferringAppSource enum defined in csd.proto.
        @IntDef({
            ReferringAppSource.REFERRING_APP_SOURCE_UNSPECIFIED,
            ReferringAppSource.KNOWN_APP_ID,
            ReferringAppSource.UNKNOWN_APP_ID,
            ReferringAppSource.ACTIVITY_REFERRER
        })
        public @interface ReferringAppSource {
            int REFERRING_APP_SOURCE_UNSPECIFIED = 0;
            int KNOWN_APP_ID = 1;
            int UNKNOWN_APP_ID = 2;
            int ACTIVITY_REFERRER = 3;
        }

        private final @ReferringAppSource int mReferringAppSource;
        private final String mReferringAppName;
        private final String mTargetUrl;
        private final String mReferringWebApkStartUrl;
        private final String mReferringWebApkManifestId;

        public ReferringAppInfo(
                @ReferringAppSource int referringAppSource,
                String referringAppName,
                String targetUrl,
                String referringWebApkStartUrl,
                String referringWebApkManifestId) {
            // Do not return null strings to native code.
            if (targetUrl == null) {
                targetUrl = "";
            }
            if (referringWebApkStartUrl == null) {
                referringWebApkStartUrl = "";
            }
            if (referringWebApkManifestId == null) {
                referringWebApkManifestId = "";
            }

            mReferringAppSource = referringAppSource;
            mReferringAppName = referringAppName;
            mTargetUrl = targetUrl;
            mReferringWebApkStartUrl = referringWebApkStartUrl;
            mReferringWebApkManifestId = referringWebApkManifestId;
        }

        @CalledByNative("ReferringAppInfo")
        public @ReferringAppSource int getSource() {
            return mReferringAppSource;
        }

        @CalledByNative("ReferringAppInfo")
        public @JniType("std::string") String getName() {
            return mReferringAppName;
        }

        @CalledByNative("ReferringAppInfo")
        public @JniType("std::string") String getTargetUrl() {
            return mTargetUrl;
        }

        @CalledByNative("ReferringAppInfo")
        public @JniType("std::string") String getReferringWebApkStartUrl() {
            return mReferringWebApkStartUrl;
        }

        @CalledByNative("ReferringAppInfo")
        public @JniType("std::string") String getReferringWebApkManifestId() {
            return mReferringWebApkManifestId;
        }
    }

    @CalledByNative
    @VisibleForTesting
    public static ReferringAppInfo getReferringAppInfo(
            WindowAndroid windowAndroid, boolean getWebApkInfo) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            return getEmptyReferringInfo();
        }

        Intent intent = activity.getIntent();
        if (intent == null) {
            return getEmptyReferringInfo();
        }

        String url = IntentHandler.getUrlFromIntent(intent);

        String referringWebApkStartUrl = "";
        String referringWebApkManifestId = "";
        if (getWebApkInfo && (activity instanceof BaseCustomTabActivity customTabActivity)) {
            WebApkExtras webApkExtras = customTabActivity.getIntentDataProvider().getWebApkExtras();
            if (webApkExtras != null) {
                referringWebApkStartUrl = webApkExtras.manifestStartUrl;
                referringWebApkManifestId = webApkExtras.manifestId;
            }
        }

        @ReferringAppInfo.ReferringAppSource
        int referringAppSource =
                ReferringAppInfo.ReferringAppSource.REFERRING_APP_SOURCE_UNSPECIFIED;
        String referringAppName = "";
        boolean foundApp = false;

        @ExternalAppId
        int externalId = IntentHandler.determineExternalIntentSource(intent, activity);
        if (externalId != ExternalAppId.OTHER) {
            referringAppSource = ReferringAppInfo.ReferringAppSource.KNOWN_APP_ID;
            referringAppName = externalAppIdToString(externalId);
            foundApp = true;
        }

        // If externalId is OTHER, fallback to EXTRA_APPLICATION_ID;
        String appId =
                foundApp
                        ? null
                        : IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID);
        if (appId != null) {
            referringAppSource = ReferringAppInfo.ReferringAppSource.UNKNOWN_APP_ID;
            referringAppName = appId;
            foundApp = true;
        }

        // If appId is empty, fallback to the referrer.
        String extraReferrer =
                foundApp ? null : IntentHandler.getActivityReferrer(intent, activity);
        if (extraReferrer != null) {
            referringAppSource = ReferringAppInfo.ReferringAppSource.ACTIVITY_REFERRER;
            referringAppName = extraReferrer;
            foundApp = true;
        }

        return new ReferringAppInfo(
                referringAppSource,
                referringAppName,
                url,
                referringWebApkStartUrl,
                referringWebApkManifestId);
    }

    private static String externalAppIdToString(@ExternalAppId int appId) {
        switch (appId) {
            case ExternalAppId.OTHER:
                return "other";
            case ExternalAppId.GMAIL:
                return "gmail";
            case ExternalAppId.FACEBOOK:
                return "facebook";
            case ExternalAppId.PLUS:
                return "plus";
            case ExternalAppId.TWITTER:
                return "twitter";
            case ExternalAppId.CHROME:
                return "chrome";
            case ExternalAppId.HANGOUTS:
                return "google.hangouts";
            case ExternalAppId.MESSENGER:
                return "android.messages";
            case ExternalAppId.NEWS:
                return "google.news";
            case ExternalAppId.LINE:
                return "line";
            case ExternalAppId.WHATSAPP:
                return "whatsapp";
            case ExternalAppId.GSA:
                return "google.search.app";
            case ExternalAppId.WEBAPK:
                return "webapk";
            case ExternalAppId.YAHOO_MAIL:
                return "yahoo.mail";
            case ExternalAppId.VIBER:
                return "viber";
            case ExternalAppId.YOUTUBE:
                return "youtube";
            case ExternalAppId.CAMERA:
                return "camera";
            case ExternalAppId.PIXEL_LAUNCHER:
                return "pixel.launcher";
            case ExternalAppId.DEPRECATED_THIRD_PARTY_LAUNCHER:
                return "third-party.launcher";
            case ExternalAppId.SAMSUNG_LAUNCHER:
                return "samsung.launcher";

            default:
                assert false : "not reached";
                return "";
        }
    }

    private static ReferringAppInfo getEmptyReferringInfo() {
        return new ReferringAppInfo(
                ReferringAppInfo.ReferringAppSource.REFERRING_APP_SOURCE_UNSPECIFIED,
                "",
                "",
                "",
                "");
    }
}
