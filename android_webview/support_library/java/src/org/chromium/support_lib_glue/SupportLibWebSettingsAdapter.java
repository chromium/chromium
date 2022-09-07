// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.webkit.WebSettings;

import org.chromium.android_webview.AwDarkMode;
import org.chromium.android_webview.AwSettings;
import org.chromium.base.Log;
import org.chromium.support_lib_boundary.WebSettingsBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/**
 * Adapter between WebSettingsBoundaryInterface and AwSettings.
 */
class SupportLibWebSettingsAdapter implements WebSettingsBoundaryInterface {
    private static final String TAG = "SupportWebSettings";
    private final AwSettings mAwSettings;

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
        if (AwDarkMode.isSimplifiedDarkModeEnabled()) {
            Log.w(TAG, "setForceDark() is a no-op in an app with targetSdkVersion>=T");
            return;
        }
        recordApiCall(ApiCall.WEB_SETTINGS_SET_FORCE_DARK);
        mAwSettings.setForceDarkMode(forceDarkMode);
    }

    @Override
    public int getForceDark() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_FORCE_DARK);
        if (AwDarkMode.isSimplifiedDarkModeEnabled()) {
            Log.w(TAG, "getForceDark() is a no-op in an app with targetSdkVersion>=T");
            return WebSettings.FORCE_DARK_AUTO;
        }
        return mAwSettings.getForceDarkMode();
    }

    @Override
    public void setForceDarkBehavior(int forceDarkBehavior) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_FORCE_DARK_BEHAVIOR);
        if (AwDarkMode.isSimplifiedDarkModeEnabled()) {
            Log.w(TAG, "setForceDarkBehavior() is a no-op in an app with targetSdkVersion>=T");
            return;
        }
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
        if (AwDarkMode.isSimplifiedDarkModeEnabled()) {
            Log.w(TAG, "getForceDarkBehavior() is a no-op in an app with targetSdkVersion>=T");
            return ForceDarkBehavior.PREFER_MEDIA_QUERY_OVER_FORCE_DARK;
        }
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

    @Override
    public void setAlgorithmicDarkeningAllowed(boolean allow) {
        if (!AwDarkMode.isSimplifiedDarkModeEnabled()) {
            Log.w(TAG,
                    "setAlgorithmicDarkeningAllowed() is a no-op in an app with"
                            + "targetSdkVersion<T");
            return;
        }
        mAwSettings.setAlgorithmicDarkeningAllowed(allow);
    }

    @Override
    public boolean isAlgorithmicDarkeningAllowed() {
        if (!AwDarkMode.isSimplifiedDarkModeEnabled()) {
            Log.w(TAG,
                    "isAlgorithmicDarkeningAllowed() is a no-op in an app with targetSdkVersion<T");
            return false;
        }
        return mAwSettings.isAlgorithmicDarkeningAllowed();
    }

    @Override
    public void setWebAuthnSupport(int support) {
        // Currently a no-op while this functionality is built out.
    }

    @Override
    public int getWebAuthnSupport() {
        // Currently a no-op while this functionality is built out.
        return WebAuthnSupport.NONE;
    }

    @Override
    public void setRequestedWithHeaderMode(int mode) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_REQUESTED_WITH_HEADER_MODE);
        switch (mode) {
            case RequestedWithHeaderMode.NO_HEADER:
                mAwSettings.setRequestedWithHeaderMode(AwSettings.REQUESTED_WITH_NO_HEADER);
                break;
            case RequestedWithHeaderMode.APP_PACKAGE_NAME:
                mAwSettings.setRequestedWithHeaderMode(AwSettings.REQUESTED_WITH_APP_PACKAGE_NAME);
                break;
        }
    }

    @Override
    public int getRequestedWithHeaderMode() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_REQUESTED_WITH_HEADER_MODE);
        // The AwSettings.REQUESTED_WITH_CONSTANT_WEBVIEW setting is intended to be internal
        // and for testing only, so it will not be mapped in the public API.
        switch (mAwSettings.getRequestedWithHeaderMode()) {
            case AwSettings.REQUESTED_WITH_NO_HEADER:
                return RequestedWithHeaderMode.NO_HEADER;
            case AwSettings.REQUESTED_WITH_APP_PACKAGE_NAME:
                return RequestedWithHeaderMode.APP_PACKAGE_NAME;
        }
        return RequestedWithHeaderMode.APP_PACKAGE_NAME;
    }

    @Override
    public void setEnterpriseAuthenticationAppLinkPolicyEnabled(boolean enabled) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_ENABLED);
        mAwSettings.setEnterpriseAuthenticationAppLinkPolicyEnabled(enabled);
    }
    @Override
    public boolean getEnterpriseAuthenticationAppLinkPolicyEnabled() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_ENABLED);
        return mAwSettings.getEnterpriseAuthenticationAppLinkPolicyEnabled();
    }
}
