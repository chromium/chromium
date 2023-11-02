// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.chromium.base.IntentUtils;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

/**
 * Implementation class for launching a {@link SettingsActivity}.
 */
public class SettingsLauncherImpl implements SettingsLauncher {
    public SettingsLauncherImpl() {}

    @Override
    public void launchSettingsActivity(Context context) {
        launchSettingsActivity(context, null);
    }

    @Override
    public void launchSettingsActivity(
            Context context, @Nullable Class<? extends Fragment> fragment) {
        launchSettingsActivity(context, fragment, null);
    }

    @Override
    public void launchSettingsActivity(Context context,
            @Nullable Class<? extends Fragment> fragment, @Nullable Bundle fragmentArgs) {
        String fragmentName = fragment != null ? fragment.getName() : null;
        Intent intent = createSettingsActivityIntent(context, fragmentName, fragmentArgs);
        IntentUtils.safeStartActivity(context, intent);
    }

    @Override
    public Intent createSettingsActivityIntent(Context context, @Nullable String fragmentName) {
        return createSettingsActivityIntent(context, fragmentName, null);
    }

    @Override
    public Intent createSettingsActivityIntent(
            Context context, @Nullable String fragmentName, @Nullable Bundle fragmentArgs) {
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
