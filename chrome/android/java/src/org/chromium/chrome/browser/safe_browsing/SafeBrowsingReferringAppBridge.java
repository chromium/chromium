// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.ExternalAppId;
import org.chromium.ui.base.WindowAndroid;

/**
 * Bridge between Java and native SafeBrowsing code to get referring app information.
 */
public class SafeBrowsingReferringAppBridge {
    private SafeBrowsingReferringAppBridge() {}

    /**
     * A helper class to store referring app information.
     */
    static class ReferringAppInfo {
        // The source of referring app name. These values must be aligned with the
        // ReferringAppSource enum defined in csd.proto.
        @IntDef({ReferringAppSource.REFERRING_APP_SOURCE_UNSPECIFIED,
                ReferringAppSource.KNOWN_APP_ID, ReferringAppSource.UNKNOWN_APP_ID,
                ReferringAppSource.ACTIVITY_REFERRER})
        public @interface ReferringAppSource {
            int REFERRING_APP_SOURCE_UNSPECIFIED = 0;
            int KNOWN_APP_ID = 1;
            int UNKNOWN_APP_ID = 2;
            int ACTIVITY_REFERRER = 3;
        }

        private final @ReferringAppSource int mReferringAppSource;
        private final String mReferringAppName;

        public ReferringAppInfo(
                @ReferringAppSource int referringAppSource, String referringAppName) {
            mReferringAppSource = referringAppSource;
            mReferringAppName = referringAppName;
        }

        @CalledByNative("ReferringAppInfo")
        public @ReferringAppSource int getSource() {
            return mReferringAppSource;
        }

        @CalledByNative("ReferringAppInfo")
        public String getName() {
            return mReferringAppName;
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

        @ExternalAppId
        int externalId = IntentHandler.determineExternalIntentSource(intent);
        if (externalId != ExternalAppId.OTHER) {
            return new ReferringAppInfo(ReferringAppInfo.ReferringAppSource.KNOWN_APP_ID,
                    externalAppIdToString(externalId));
        }

        // If externalId is OTHER, fallback to EXTRA_APPLICATION_ID;
        String appId = IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID);
        if (appId != null) {
            return new ReferringAppInfo(ReferringAppInfo.ReferringAppSource.UNKNOWN_APP_ID, appId);
        }

        // If appId is empty, fallback to EXTRA_REFERRER;
        // If the activity is launched through launcher activity, the referrer is set through
        // intent extra.
        String activity_referrer =
                IntentUtils.safeGetStringExtra(intent, IntentHandler.EXTRA_ACTIVITY_REFERRER);
        if (activity_referrer != null) {
            return new ReferringAppInfo(
                    ReferringAppInfo.ReferringAppSource.ACTIVITY_REFERRER, activity_referrer);
        }

        // If the activity referrer is not found in intent extra, get it from the activity
        // directly.
        Uri extraReferrer = activity.getReferrer();
        if (extraReferrer != null) {
            return new ReferringAppInfo(ReferringAppInfo.ReferringAppSource.ACTIVITY_REFERRER,
                    extraReferrer.toString());
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
            default:
                assert false : "not reached";
                return "";
        }
    }

    private static ReferringAppInfo getEmptyReferringInfo() {
        return new ReferringAppInfo(
                ReferringAppInfo.ReferringAppSource.REFERRING_APP_SOURCE_UNSPECIFIED, "");
    }
}
