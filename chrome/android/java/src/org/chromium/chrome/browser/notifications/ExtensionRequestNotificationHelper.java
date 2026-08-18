// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;

import java.util.ArrayList;

/** Helper class for managing extension request notifications on Android. */
@NullMarked
public class ExtensionRequestNotificationHelper {
    private ExtensionRequestNotificationHelper() {}

    /**
     * Launches the Chrome Web Store details page for each approved extension in Chrome.
     *
     * @param extensionIds The array of extension IDs to open.
     */
    @CalledByNative
    public static void launchWebStoreUrls(
            @JniType("std::vector<std::string>") String[] extensionIds) {
        if (extensionIds.length == 0) return;
        Context context = ContextUtils.getApplicationContext();

        ArrayList<String> urls = new ArrayList<>();
        for (String extensionId : extensionIds) {
            String trimmedId = extensionId.trim();
            if (!trimmedId.isEmpty()) {
                urls.add("https://chromewebstore.google.com/detail/" + trimmedId);
            }
        }
        if (urls.isEmpty()) return;

        Intent intent =
                new Intent()
                        .setAction(Intent.ACTION_VIEW)
                        .setData(Uri.parse(urls.get(0)))
                        .setClass(context, ChromeLauncherActivity.class)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                        .putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName())
                        .putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);

        if (urls.size() > 1) {
            ArrayList<String> additionalUrls = new ArrayList<>(urls.subList(1, urls.size()));
            intent.putExtra(IntentHandler.EXTRA_ADDITIONAL_URLS, additionalUrls);
        }

        IntentUtils.addTrustedIntentExtras(intent);
        context.startActivity(intent);
    }
}
