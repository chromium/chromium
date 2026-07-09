// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.settings.SettingsIntentUtil;

/** A helper activity for launching Chrome settings from intents of other apps. */
@NullMarked
public final class AutofillOptionsLauncher extends Activity {
    private static final String TAG = "AutofillOptLauncher";

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (invokedByPreferencesIntent()) {
            startActivity(createAutofillOptionsIntent());
        } else {
            Log.e(TAG, "Dropping intent: Handles only APPLICATION_PREFERENCE intents.");
        }

        finish();
    }

    private boolean invokedByPreferencesIntent() {
        return getIntent() != null
                && Intent.ACTION_APPLICATION_PREFERENCES.equals(getIntent().getAction());
    }

    private Intent createAutofillOptionsIntent() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(
                AutofillOptionsFragment.AUTOFILL_OPTIONS_REFERRER,
                AutofillOptionsReferrer.DEEP_LINK_TO_SETTINGS);
        return SettingsIntentUtil.createIntent(
                this, AutofillOptionsFragment.class.getName(), fragmentArgs);
    }
}
