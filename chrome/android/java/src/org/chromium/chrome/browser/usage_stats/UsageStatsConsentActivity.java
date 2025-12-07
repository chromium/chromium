// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.app.Activity;
import android.content.ComponentName;
import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.profiles.Profile;

/** Activity that prompts the user for consent to share browsing activity with Digital Wellbeing. */
@NullMarked
public class UsageStatsConsentActivity extends SynchronousInitializationActivity {
    public static final String UNAUTHORIZE_ACTION =
            "org.chromium.chrome.browser.usage_stats.action.UNAUTHORIZE";

    private static final String DIGITAL_WELLBEING_PACKAGE_NAME =
            "com.google.android.apps.wellbeing";

    @Override
    protected void onCreateInternal(@Nullable Bundle savedInstanceState) {
        super.onCreateInternal(savedInstanceState);
        ComponentName caller = getCallingActivity();
        if (caller == null
                || !TextUtils.equals(DIGITAL_WELLBEING_PACKAGE_NAME, caller.getPackageName())) {
            finish();
            return;
        }
    }

    @Override
    public void onAttachedToWindow() {
        getProfileSupplier().runSyncOrOnAvailable(this::showConsentDialog);
    }

    private void showConsentDialog(Profile profile) {
        String action = getIntent().getAction();
        boolean isRevocation = TextUtils.equals(action, UNAUTHORIZE_ACTION);

        UsageStatsConsentDialog.create(
                        this,
                        profile,
                        isRevocation,
                        (didConfirm) -> {
                            setResult(didConfirm ? Activity.RESULT_OK : Activity.RESULT_CANCELED);
                            finish();
                        })
                .show();
    }
}
