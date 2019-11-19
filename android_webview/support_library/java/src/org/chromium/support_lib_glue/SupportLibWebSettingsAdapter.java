// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwSettings;
import org.chromium.support_lib_boundary.WebSettingsBoundaryInterface;

/**
 * Adapter between WebSettingsBoundaryInterface and AwSettings.
 */
class SupportLibWebSettingsAdapter implements WebSettingsBoundaryInterface {
    private final AwSettings mAwSettings;

    public SupportLibWebSettingsAdapter(AwSettings awSettings) {
        mAwSettings = awSettings;
    }

    @Override
    public void setOffscreenPreRaster(boolean enabled) {
        mAwSettings.setOffscreenPreRaster(enabled);
    }

    @Override
    public boolean getOffscreenPreRaster() {
        return mAwSettings.getOffscreenPreRaster();
    }

    @Override
    public void setSafeBrowsingEnabled(boolean enabled) {
        mAwSettings.setSafeBrowsingEnabled(enabled);
    }

    @Override
    public boolean getSafeBrowsingEnabled() {
        return mAwSettings.getSafeBrowsingEnabled();
    }

    @Override
    public void setDisabledActionModeMenuItems(int menuItems) {
        mAwSettings.setDisabledActionModeMenuItems(menuItems);
    }

    @Override
    public int getDisabledActionModeMenuItems() {
        return mAwSettings.getDisabledActionModeMenuItems();
    }

    @Override
    public boolean getWillSuppressErrorPage() {
        return mAwSettings.getWillSuppressErrorPage();
    }

    @Override
    public void setWillSuppressErrorPage(boolean suppressed) {
        mAwSettings.setWillSuppressErrorPage(suppressed);
    }

    @Override
    public void setForceDark(int forceDarkMode) {
        mAwSettings.setForceDarkMode(forceDarkMode);
    }

    @Override
    public int getForceDark() {
        return mAwSettings.getForceDarkMode();
    }

    @Override
    public void setForceDarkBehavior(int forceDarkBehavior) {
        switch (forceDarkBehavior) {
            case ForceDarkBehavior.FORCE_DARK_ONLY:
                mAwSettings.setForceDarkBehavior(AwSettings.FORCE_DARK_ONLY);
                break;
            case ForceDarkBehavior.MEDIA_QUERY_ONLY:
                mAwSettings.setForceDarkBehavior(AwSettings.MEDIA_QUERY_ONLY);
                break;
            case ForceDarkBehavior.PREFER_MEDIA_QUERY_OVER_FORCE_DARK:
                mAwSettings.setForceDarkBehavior(AwSettings.PREFER_MEDIA_QUERY_OVER_FORCE_DARK);
                break;
        }
    }

    @Override
    public int getForceDarkBehavior() {
        switch (mAwSettings.getForceDarkBehavior()) {
            case AwSettings.FORCE_DARK_ONLY:
                return ForceDarkBehavior.FORCE_DARK_ONLY;
            case AwSettings.MEDIA_QUERY_ONLY:
                return ForceDarkBehavior.MEDIA_QUERY_ONLY;
            case AwSettings.PREFER_MEDIA_QUERY_OVER_FORCE_DARK:
                return ForceDarkBehavior.PREFER_MEDIA_QUERY_OVER_FORCE_DARK;
        }
        return ForceDarkBehavior.PREFER_MEDIA_QUERY_OVER_FORCE_DARK;
    }
}
