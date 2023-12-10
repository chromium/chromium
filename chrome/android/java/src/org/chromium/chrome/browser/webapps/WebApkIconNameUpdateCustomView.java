// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;

/**
 * The custom view part of the {@link WebApkIconNameUpdateDialog}. Shows the icon changes and
 * changes to name and short_name.
 */
public class WebApkIconNameUpdateCustomView extends LinearLayout {
    /** Constructor for inflating from XMLs. */
    public WebApkIconNameUpdateCustomView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Setup the views showing the two icons (before and after).
     *
     * @param oldIcon The icon of the currently installed app.
     * @param newIcon The proposed new icon for the updated app.
     * @param oldIconAdaptive Wheter the current icon is adaptive.
     * @param newIconAdaptive Wheter the updated icon is adaptive.
     */
    public void configureIcons(
            Bitmap oldIcon, Bitmap newIcon, boolean oldIconAdaptive, boolean newIconAdaptive) {
        ImageView oldIconView = findViewById(R.id.app_icon_old);
        ImageView newIconView = findViewById(R.id.app_icon_new);
        if (oldIconAdaptive && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            oldIconView.setImageIcon(Icon.createWithAdaptiveBitmap(oldIcon));
        } else {
            oldIconView.setImageBitmap(oldIcon);
        }
        if (newIconAdaptive && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            newIconView.setImageIcon(Icon.createWithAdaptiveBitmap(newIcon));
        } else {
            newIconView.setImageBitmap(newIcon);
        }
        oldIconView.setVisibility(View.VISIBLE);
        newIconView.setVisibility(View.VISIBLE);
    }

    /**
     * Setup the short app names text views.
     *
     * @param oldAppShortName The short name of the currently installed app.
     * @param newAppShortName The proposed short name for the updated app.
     */
    public void configureShortNames(String oldAppShortName, String newAppShortName) {
        TextView currentShortName = findViewById(R.id.short_app_name_old);
        TextView updatedShortName = findViewById(R.id.short_app_name_new);
        currentShortName.setText(oldAppShortName);
        updatedShortName.setText(newAppShortName);
        currentShortName.setVisibility(View.VISIBLE);
        updatedShortName.setVisibility(View.VISIBLE);
    }

    /**
     * Setup the app names text views.
     *
     * @param oldAppName The name of the currently installed app.
     * @param newAppName The proposed name for the updated app.
     */
    public void configureNames(String oldAppName, String newAppName) {
        TextView currentLongName = findViewById(R.id.app_name_old);
        TextView updatedLongName = findViewById(R.id.app_name_new);
        currentLongName.setText(oldAppName);
        updatedLongName.setText(newAppName);
        currentLongName.setVisibility(View.VISIBLE);
        updatedLongName.setVisibility(View.VISIBLE);
    }
}
