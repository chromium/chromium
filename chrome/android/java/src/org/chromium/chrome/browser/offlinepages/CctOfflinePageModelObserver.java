// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.AppHooks;

/**
 * Java-side handler for Offline page model changes.
 *
 * Will send a broadcast intent to the originating app if a page related to it has changed
 * and the app is part of the whitelist set of apps.
 */
public class CctOfflinePageModelObserver {
    private static final String TAG = "CctModelObserver";
    // Key for pending intent used by receivers to verify origin.
    private static final String ORIGIN_VERIFICATION_KEY =
            "org.chromium.chrome.extra.CHROME_NAME_PENDING_INTENT";

    // Key for bundle which stores information about the page changed.
    @VisibleForTesting
    static final String PAGE_INFO_KEY = "org.chromium.chrome.extra.OFFLINE_PAGE_INFO";
    // Key within page info bundle for whether the page was added (true) or removed (false).
    @VisibleForTesting
    static final String IS_NEW_KEY = "is_new";
    // Key within page info bundle for online url.
    @VisibleForTesting
    static final String URL_KEY = "url";
    // Broadcast action.
    @VisibleForTesting
    static final String ACTION_OFFLINE_PAGES_UPDATED =
            "org.chromium.chrome.browser.offlinepages.OFFLINE_PAGES_CHANGED";

    @CalledByNative
    @VisibleForTesting
    static void onPageChanged(String originString, boolean isNew, String url) {
        OfflinePageOrigin origin = new OfflinePageOrigin(originString);
        if (origin.isChrome()) return;
        Bundle bundle = new Bundle();
        bundle.putBoolean(IS_NEW_KEY, isNew);
        bundle.putString(URL_KEY, url);
        compareSignaturesAndFireIntent(origin, bundle);
    }

    private static void compareSignaturesAndFireIntent(OfflinePageOrigin origin, Bundle pageInfo) {
        if (!isInWhitelist(origin.getAppName())) {
            Log.w(TAG, "Non-whitelisted app: " + origin.getAppName());
            return;
        }
        Context context = ContextUtils.getApplicationContext();
        if (!origin.doesSignatureMatch(context)) {
            Log.w(TAG, "Signature hashes are different");
            return;
        }
        // Create broadcast if signatures match.
        Intent intent = new Intent();
        intent.setAction(ACTION_OFFLINE_PAGES_UPDATED);
        intent.setPackage(origin.getAppName());

        // Create a pending intent and cancel it, as this is only expected to verify
        // that chrome created the OFFLINE_PAGES_UPDATED broadcast, not for actual use.
        PendingIntent originVerification = PendingIntent.getBroadcast(
                context, 0, new Intent(), PendingIntent.FLAG_UPDATE_CURRENT);
        originVerification.cancel();

        intent.putExtra(ORIGIN_VERIFICATION_KEY, originVerification);
        intent.putExtra(PAGE_INFO_KEY, pageInfo);
        context.sendBroadcast(intent);
    }

    private static boolean isInWhitelist(String appName) {
        return AppHooks.get().getOfflinePagesCctWhitelist().contains(appName);
    }
}
