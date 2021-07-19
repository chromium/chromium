// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.content.res.Configuration;

import org.chromium.android_webview.AwSettings;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.support_lib_boundary.WebSettingsBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/**
 * Adapter between WebSettingsBoundaryInterface and AwSettings.
 */
class SupportLibWebSettingsAdapter implements WebSettingsBoundaryInterface {
    private final AwSettings mAwSettings;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    private static final int OS_NIGHT_MODE_UNDEFINED = 0 * AwSettings.FORCE_DARK_MODES_COUNT;
    private static final int OS_NIGHT_MODE_NO = 1 * AwSettings.FORCE_DARK_MODES_COUNT;
    private static final int OS_NIGHT_MODE_YES = 2 * AwSettings.FORCE_DARK_MODES_COUNT;
    private static final int UMA_FORCE_DARK_MODE_MAX_VALUE = 3 * AwSettings.FORCE_DARK_MODES_COUNT;

    public SupportLibWebSettingsAdapter(AwSettings awSettings) {
        mAwSettings = awSettings;
    }

    @Override
    public void setOffscreenPreRaster(boolean enabled) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_OFFSCREEN_PRE_RASTER);
        mAwSettings.setOffscreenPreRaster(enabled);
    }

    @Override
    public boolean getOffscreenPreRaster() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_OFFSCREEN_PRE_RASTER);
        return mAwSettings.getOffscreenPreRaster();
    }

    @Override
    public void setSafeBrowsingEnabled(boolean enabled) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_SAFE_BROWSING_ENABLED);
        mAwSettings.setSafeBrowsingEnabled(enabled);
    }

    @Override
    public boolean getSafeBrowsingEnabled() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_SAFE_BROWSING_ENABLED);
        return mAwSettings.getSafeBrowsingEnabled();
    }

    @Override
    public void setDisabledActionModeMenuItems(int menuItems) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_DISABLED_ACTION_MODE_MENU_ITEMS);
        mAwSettings.setDisabledActionModeMenuItems(menuItems);
    }

    @Override
    public int getDisabledActionModeMenuItems() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_DISABLED_ACTION_MODE_MENU_ITEMS);
        return mAwSettings.getDisabledActionModeMenuItems();
    }

    @Override
    public boolean getWillSuppressErrorPage() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_WILL_SUPPRESS_ERROR_PAGE);
        return mAwSettings.getWillSuppressErrorPage();
    }

    @Override
    public void setWillSuppressErrorPage(boolean suppressed) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_WILL_SUPPRESS_ERROR_PAGE);
        mAwSettings.setWillSuppressErrorPage(suppressed);
    }

    @Override
    public void setForceDark(int forceDarkMode) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_FORCE_DARK);
        recordForceDarkMode(forceDarkMode);
        mAwSettings.setForceDarkMode(forceDarkMode);
    }

    @Override
    public int getForceDark() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_FORCE_DARK);
        return mAwSettings.getForceDarkMode();
    }

    @Override
    public void setForceDarkBehavior(int forceDarkBehavior) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_FORCE_DARK_BEHAVIOR);
        recordForceDarkBehavior(forceDarkBehavior);
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
        recordApiCall(ApiCall.WEB_SETTINGS_GET_FORCE_DARK_BEHAVIOR);
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

    private void recordForceDarkMode(int forceDarkMode) {
        int value = getOsNightMode() + forceDarkMode;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.ForceDarkMode", value, UMA_FORCE_DARK_MODE_MAX_VALUE);
    }

    private void recordForceDarkBehavior(int forceDarkBehavior) {
        RecordHistogram.recordEnumeratedHistogram("Android.WebView.ForceDarkBehavior",
                forceDarkBehavior, AwSettings.FORCE_DARK_STRATEGY_COUNT);
    }

    private int getOsNightMode() {
        int osNightMode = mAwSettings.getUiModeNight();
        switch (osNightMode) {
            case Configuration.UI_MODE_NIGHT_NO:
                return OS_NIGHT_MODE_NO;
            case Configuration.UI_MODE_NIGHT_YES:
                return OS_NIGHT_MODE_YES;
            case Configuration.UI_MODE_NIGHT_UNDEFINED:
            default:
                return OS_NIGHT_MODE_UNDEFINED;
        }
    }
}
