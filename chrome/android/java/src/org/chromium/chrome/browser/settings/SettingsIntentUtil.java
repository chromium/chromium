// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public class SettingsIntentUtil {
    private SettingsIntentUtil() {}

    /**
     * Creates an {@link Intent} that launches the settings activity.
     *
     * @param context The context from which the settings activity is being launched.
     * @param fragmentName The name of the main fragment shown in the settings activity. null means
     *     the default fragment.
     * @param fragmentArgs A bundle of extra arguments given to the main fragment. Can be null.
     * @return An intent ready to launch the settings activity.
     */
    public static Intent createIntent(
            @NonNull Context context,
            @Nullable String fragmentName,
            @Nullable Bundle fragmentArgs) {
        Intent intent = new Intent();
        intent.setClass(context, SettingsActivity.class);
        if (!(context instanceof Activity)) {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        }
        if (fragmentName != null) {
            intent.putExtra(SettingsActivity.EXTRA_SHOW_FRAGMENT, fragmentName);
        }
        if (fragmentArgs != null) {
            intent.putExtra(SettingsActivity.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        }
        return intent;
    }
}
