// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

/** Data class containing the info about an app. */
class AppInfo {
    private static final AppInfo EMPTY_INFO = new AppInfo(null, null, null);

    @Nullable private final Drawable mIcon;
    @Nullable private final CharSequence mLabel;
    @Nullable private final String mId;

    public static AppInfo create(PackageManager pm, @Nullable String id) {
        if (id == null) return EMPTY_INFO;

        try {
            var info = pm.getApplicationInfo(id, PackageManager.GET_META_DATA);
            var icon = pm.getApplicationIcon(info);
            var label = pm.getApplicationLabel(info);
            return new AppInfo(id, icon, label);
        } catch (NameNotFoundException e) {
            // Can happen if the corresponding app was uninstalled, or unavailable for any reason.
            return EMPTY_INFO;
        }
    }

    public AppInfo(String id, Drawable icon, CharSequence label) {
        mId = id;
        mIcon = icon;
        mLabel = label;
    }

    public Drawable getIcon() {
        return mIcon;
    }

    public CharSequence getLabel() {
        return mLabel;
    }

    /** Return whether the app info object is valid. */
    public boolean isValid() {
        return mId != null;
    }
}
