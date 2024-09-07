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

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.ExternalAppId;
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

        public ReferringAppInfo(
                @ReferringAppSource int referringAppSource,
                String referringAppName,
                String targetUrl) {
            mReferringAppSource = referringAppSource;
            mReferringAppName = referringAppName;
            mTargetUrl = targetUrl;
        }

        @CalledByNative("ReferringAppInfo")
        public @ReferringAppSource int getSource() {
            return mReferringAppSource;
        }

        @CalledByNative("ReferringAppInfo")
        public String getName() {
            return mReferringAppName;
        }

        @CalledByNative("ReferringAppInfo")
        public String getTargetUrl() {
            return mTargetUrl;
        }
    }

    @CalledByNative
    @VisibleForTesting
    public static ReferringAppInfo getReferringAppInfo(WindowAndroid windowAndroid) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            return getEmptyReferringInfo();
        }

        Intent intent = activity.getIntent();
        if (intent == null) {
            return getEmptyReferringInfo();
        }

        String url = IntentHandler.getUrlFromIntent(intent);
        if (url == null) {
            // `url` is returned to native code. Rather than handling
            // null strings on the native side, we return an empty
            // string.
            url = "";
        }

        @ExternalAppId
        int externalId = IntentHandler.determineExternalIntentSource(intent, activity);
        if (externalId != ExternalAppId.OTHER) {
            return new ReferringAppInfo(
                    ReferringAppInfo.ReferringAppSource.KNOWN_APP_ID,
                    externalAppIdToString(externalId),
                    url);
        }

        // If externalId is OTHER, fallback to EXTRA_APPLICATION_ID;
        String appId = IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID);
        if (appId != null) {
            return new ReferringAppInfo(
                    ReferringAppInfo.ReferringAppSource.UNKNOWN_APP_ID, appId, url);
        }

        // If appId is empty, fallback to the referrer.
        String extraReferrer = IntentHandler.getActivityReferrer(intent, activity);
        if (extraReferrer != null) {
            return new ReferringAppInfo(
                    ReferringAppInfo.ReferringAppSource.ACTIVITY_REFERRER, extraReferrer, url);
        }

        return getEmptyReferringInfo();
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
            default:
                assert false : "not reached";
                return "";
        }
    }

    private static ReferringAppInfo getEmptyReferringInfo() {
        return new ReferringAppInfo(
                ReferringAppInfo.ReferringAppSource.REFERRING_APP_SOURCE_UNSPECIFIED, "", "");
    }
}
