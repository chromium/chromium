// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;

/** Utility class for Glic UI components. */
@NullMarked
public final class GlicUiUtils {
    private GlicUiUtils() {}

    /**
     * Configures the Glic inactive placeholder view.
     *
     * @param placeholder The placeholder view.
     */
    public static void setupPlaceholderView(TextViewWithCompoundDrawables placeholder) {
        placeholder.setText(R.string.glic_inactive_view_card_text);
        placeholder.setCompoundDrawablesRelativeWithIntrinsicBounds(
                0, R.drawable.ic_spark_filled_24dp, 0, 0);
        placeholder.setDrawableTintColor(
                AppCompatResources.getColorStateList(
                        placeholder.getContext(), R.color.default_icon_color_tint_list));
    }

    /**
     * Opens the Android "App info" settings screen for Chrome, where app permissions such as the
     * microphone can be managed.
     *
     * @param context The Android {@link Context} used to start the activity.
     */
    public static void openAppDetailsSettings(Context context) {
        Intent intent =
                new Intent(
                        Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                        Uri.parse("package:" + context.getPackageName()));
        // Starting an activity from a non-activity context requires it to be in its own task.
        if (!(context instanceof Activity)) {
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        }
        IntentUtils.safeStartActivity(context, intent);
    }
}
