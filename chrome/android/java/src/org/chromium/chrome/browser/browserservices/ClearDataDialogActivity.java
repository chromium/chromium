// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.util.IntentUtils;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Activity that shows a dialog inviting the user to clear the browsing data for the origins
 * associated with Trusted Web Activity client app, which has just been uninstalled or had its data
 * cleared.
 */
public class ClearDataDialogActivity extends AppCompatActivity {
    private static final String EXTRA_APP_NAME = "org.chromium.chrome.extra.app_name";
    private static final String EXTRA_DOMAINS = "org.chromium.chrome.extra.domains";
    private static final String EXTRA_ORIGINS = "org.chromium.chrome.extra.origins";
    private static final String EXTRA_APP_UNINSTALLED = "org.chromium.chrome.extra.app_uninstalled";

    /**
     * Creates an intent for launching this activity for the TWA client app that was uninstalled or
     * had its data cleared.
     */
    public static Intent createIntent(Context context, String appName,
            Collection<String> linkedDomains, Collection<String> linkedOrigins,
            boolean appUninstalled) {
        Intent intent = new Intent(context, ClearDataDialogActivity.class);
        intent.putExtra(EXTRA_APP_NAME, appName);
        intent.putExtra(EXTRA_DOMAINS, new ArrayList<>(linkedDomains));
        intent.putExtra(EXTRA_ORIGINS, new ArrayList<>(linkedOrigins));
        intent.putExtra(EXTRA_APP_UNINSTALLED, appUninstalled);
        return intent;
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        AlertDialog.Builder builder = new AlertDialog.Builder(this,
                android.R.style.Theme_DeviceDefault_Light_Dialog_Alert)
                .setTitle(getString(R.string.twa_clear_data_dialog_title,
                        getAppNameFromIntent(getIntent())))
                .setMessage(R.string.twa_clear_data_dialog_message)
                .setPositiveButton(R.string.preferences, (ignored1, ignored2) -> {
                    recordDecision(true);
                    openSettings();
                    finish();
                })
                .setNegativeButton(R.string.twa_clear_data_dialog_keep_data,
                        (ignored1, ignored2) -> {
                    recordDecision(false);
                    finish();
                })
                .setOnCancelListener((ignored) -> {
                    recordDecision(false);
                    finish();
                });

        builder.create().show();
    }

    private void openSettings() {
        List<String> origins = getOriginsFromIntent(getIntent());
        List<String> domains = getDomainsFromIntent(getIntent());
        if (origins == null || origins.isEmpty() || domains == null || domains.isEmpty()) {
            assert false : "Invalid extras for ClearDataDialogActivity";
            return;
        }
        TrustedWebActivitySettingsLauncher.launch(this, origins, domains);
    }

    private void recordDecision(boolean accepted) {
        boolean appUninstalled = getIsAppUninstalledFromIntent(getIntent());
        ChromeApplication.getComponent().resolveTwaClearDataDialogRecorder()
                .handleDialogResult(accepted, appUninstalled);
    }

    @VisibleForTesting
    static List<String> getOriginsFromIntent(Intent intent) {
        return IntentUtils.safeGetStringArrayListExtra(intent, EXTRA_ORIGINS);
    }

    @VisibleForTesting
    static List<String> getDomainsFromIntent(Intent intent) {
        return IntentUtils.safeGetStringArrayListExtra(intent, EXTRA_DOMAINS);
    }

    @VisibleForTesting
    static boolean getIsAppUninstalledFromIntent(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_APP_UNINSTALLED, false);
    }

    @VisibleForTesting
    static String getAppNameFromIntent(Intent intent) {
        return IntentUtils.safeGetStringExtra(intent, EXTRA_APP_NAME);
    }
}
