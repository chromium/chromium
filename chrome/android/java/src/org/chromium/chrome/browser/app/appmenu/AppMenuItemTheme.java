// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.MenuItem;

import androidx.annotation.ColorRes;
import androidx.annotation.IdRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.sync.UserActionableError;

/** Class that handles theming of AppMenuItems. */
@NullMarked
public class AppMenuItemTheme {
    private final Context mContext;
    private final TabModelSelector mTabModelSelector;

    public AppMenuItemTheme(Context context, TabModelSelector tabModelSelector) {
        mContext = context;
        mTabModelSelector = tabModelSelector;
    }

    /** Return whether the given {@link MenuItem} is managed by policy. */
    public boolean isMenuItemManaged(@IdRes int itemId) {
        if (itemId == R.id.new_incognito_tab_menu_id
                || itemId == R.id.new_incognito_window_menu_id) {
            return IncognitoUtils.isIncognitoModeManaged(
                    assumeNonNull(mTabModelSelector.getCurrentModel().getProfile()));
        }
        return false;
    }

    /** Returns true if a badge (i.e. a red-dot) should be shown on the menu item icon. */
    public boolean shouldShowBadgeOnMenuItemIcon(@IdRes int itemId) {
        if (itemId == R.id.preferences_id) {
            // Theoretically mTabModelSelector could return a stub model.
            @Nullable Profile profile = mTabModelSelector.getCurrentModel().getProfile();
            if (profile == null) {
                return false;
            }
            // Return true if there is any error.
            return SyncSettingsUtils.getSyncError(profile) != UserActionableError.NONE;
        }
        return false;
    }

    /**
     * Returns content description for the menu item, if different from the titleCondensed xml
     * attribute.
     */
    public @Nullable String getContentDescription(@IdRes int itemId) {
        if (itemId == R.id.preferences_id) {
            // Theoretically mTabModelSelector could return a stub model.
            @Nullable Profile profile = mTabModelSelector.getCurrentModel().getProfile();
            if (profile == null) {
                return null;
            }
            if (SyncSettingsUtils.getSyncError(profile) != UserActionableError.NONE) {
                return mContext.getString(R.string.menu_settings_account_error);
            }
        }
        return null;
    }

    /** Returns whether the menu item's icon need to be tinted to blue. */
    public @ColorRes int getMenuItemIconColorRes(@IdRes int itemId) {
        if (itemId == R.id.disable_price_tracking_menu_id) {
            return R.color.default_icon_color_accent1_tint_list;
        }
        return R.color.default_icon_color_secondary_tint_list;
    }
}
