// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

/** Utility methods for the browsing history manager. */
public class HistoryManagerUtils {
    public static final int HISTORY_REQUEST_CODE = 723649;

    /**
     * Opens the browsing history manager.
     *
     * @param activity The {@link Activity} that owns the {@link HistoryManager}.
     * @param tab The {@link Tab} to used to display the native page version of the {@link
     *     HistoryManager}.
     * @param isIncognitoSelected Whether the incognito {@TabModelSelector} is selected.
     */
    public static void showHistoryManager(Activity activity, Tab tab, boolean isIncognitoSelected) {
        Context appContext = ContextUtils.getApplicationContext();
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)) {
            // History shows up as a tab on tablets.
            LoadUrlParams params = new LoadUrlParams(UrlConstants.NATIVE_HISTORY_URL);
            tab.loadUrl(params);
        } else {
            Intent intent = new Intent();
            intent.setClass(appContext, HistoryActivity.class);
            intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
            intent.putExtra(IntentHandler.EXTRA_INCOGNITO_MODE, isIncognitoSelected);
            activity.startActivity(intent);
        }
    }

    /**
     * Opens the app specific history manager. For launching history for CCTs, using
     * startActivityForResult to ensure identity sharing.
     *
     * @param activity The {@link Activity} that owns the {@link HistoryManager}.
     * @param isIncognitoSelected Whether the incognito {@TabModelSelector} is selected.
     * @param clientPackageName Package name of the client from which the history activity is
     *     launched.
     */
    // TODO(katzz): Convert to ActivityResult API
    public static void showAppSpecificHistoryManager(
            Activity activity, boolean isIncognitoSelected, String clientPackageName) {
        Intent intent = new Intent();
        intent.setClass(activity, HistoryActivity.class);
        intent.putExtra(IntentHandler.EXTRA_INCOGNITO_MODE, isIncognitoSelected);
        intent.putExtra(IntentHandler.EXTRA_APP_SPECIFIC_HISTORY, true);
        intent.putExtra(Intent.EXTRA_PACKAGE_NAME, clientPackageName);
        activity.startActivityForResult(intent, HISTORY_REQUEST_CODE);
    }
}
