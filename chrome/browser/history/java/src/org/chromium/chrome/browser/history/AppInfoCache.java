// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.history.FilterSheetCoordinator.FilterItem;

import java.util.HashMap;

/** Caches app label and icon metadata keyed by package name for app-specific history. */
@NullMarked
class AppInfoCache {
    private static final FilterItem<String> INVALID_ITEM =
            new FilterItem<>("", /* icon= */ null, "");
    private @Nullable HashMap<String, FilterItem<String>> mAppInfoMap;
    private PackageManager mPackageManager;

    public AppInfoCache(PackageManager packageManager) {
        mPackageManager = packageManager;
    }

    public @Nullable FilterItem<String> get(String appId) {
        if (mAppInfoMap == null) mAppInfoMap = new HashMap<>();
        FilterItem<String> appInfo = mAppInfoMap.get(appId);
        if (appInfo == null) {
            try {
                PackageManager pm = mPackageManager;
                ApplicationInfo info = pm.getApplicationInfo(appId, PackageManager.GET_META_DATA);
                Drawable icon = pm.getApplicationIcon(info);
                CharSequence label = pm.getApplicationLabel(info);
                appInfo = new FilterItem<>(appId, icon, label);
            } catch (NameNotFoundException e) {
                // Can happen if the corresponding app was uninstalled, or unavailable for any
                // reason. Map it with an invalid info so it won't be queried again till next
                // time history UI is launched.
                appInfo = INVALID_ITEM;
            }
            mAppInfoMap.put(appId, appInfo);
        }
        return appInfo == INVALID_ITEM ? null : appInfo;
    }

    void setPackageManagerForTesting(PackageManager packageManager) {
        mPackageManager = packageManager;
    }
}
