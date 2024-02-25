// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.SuppressLint;
import android.webkit.WebSettings;
import android.webkit.WebSettings.LayoutAlgorithm;
import android.webkit.WebSettings.PluginState;
import android.webkit.WebSettings.RenderPriority;
import android.webkit.WebSettings.ZoomDensity;

import com.android.webview.chromium.WebViewChromium.ApiCall;

import org.chromium.android_webview.AwDarkMode;
import org.chromium.android_webview.AwSettings;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;

/**
 * Type adaptation layer between {@link android.webkit.WebSettings} and
 * {@link org.chromium.android_webview.AwSettings}.
 */
@SuppressWarnings({"deprecation", "NoSynchronizedMethodCheck"})
public class ContentSettingsAdapter extends android.webkit.WebSettings {
    private static final String TAG = "WebSettings";
    private AwSettings mAwSettings;
    private PluginState mPluginState = PluginState.OFF;

    public ContentSettingsAdapter(AwSettings awSettings) {
        mAwSettings = awSettings;
    }

    AwSettings getAwSettings() {
        return mAwSettings;
    }

    @Override
    @Deprecated
    public void setNavDump(boolean enabled) {
        // Intentional no-op.
    }

    @Override
    @Deprecated
    public boolean getNavDump() {
        // Intentional no-op.
        return false;
    }

    @Override
    public void setSupportZoom(boolean support) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_SUPPORT_ZOOM")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_SUPPORT_ZOOM);
            mAwSettings.setSupportZoom(support);
        }
    }

    @Override
    public boolean supportZoom() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SUPPORT_ZOOM")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SUPPORT_ZOOM);
            return mAwSettings.supportZoom();
        }
    }

    @Override
    public void setBuiltInZoomControls(boolean enabled) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_BUILT_IN_ZOOM_CONTROLS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_BUILT_IN_ZOOM_CONTROLS);
            mAwSettings.setBuiltInZoomControls(enabled);
        }
    }

    @Override
    public boolean getBuiltInZoomControls() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_BUILT_IN_ZOOM_CONTROLS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_BUILT_IN_ZOOM_CONTROLS);
            return mAwSettings.getBuiltInZoomControls();
        }
    }

    @Override
    public void setDisplayZoomControls(boolean enabled) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_DISPLAY_ZOOM_CONTROLS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_DISPLAY_ZOOM_CONTROLS);
            mAwSettings.setDisplayZoomControls(enabled);
        }
    }

    @Override
    public boolean getDisplayZoomControls() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_DISPLAY_ZOOM_CONTROLS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_DISPLAY_ZOOM_CONTROLS);
            return mAwSettings.getDisplayZoomControls();
        }
    }

    @Override
    public void setAllowFileAccess(boolean allow) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_ALLOW_FILE_ACCESS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_ALLOW_FILE_ACCESS);
            mAwSettings.setAllowFileAccess(allow);
        }
    }

    @Override
    public boolean getAllowFileAccess() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_GET_ALLOW_FILE_ACCESS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_ALLOW_FILE_ACCESS);
            return mAwSettings.getAllowFileAccess();
        }
    }

    @Override
    public void setAllowContentAccess(boolean allow) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_ALLOW_CONTENT_ACCESS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_ALLOW_CONTENT_ACCESS);
            mAwSettings.setAllowContentAccess(allow);
        }
    }

    @Override
    public boolean getAllowContentAccess() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_ALLOW_CONTENT_ACCESS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_ALLOW_CONTENT_ACCESS);
            return mAwSettings.getAllowContentAccess();
        }
    }

    @Override
    public void setLoadWithOverviewMode(boolean overview) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_LOAD_WITH_OVERVIEW_MODE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_LOAD_WITH_OVERVIEW_MODE);
            mAwSettings.setLoadWithOverviewMode(overview);
        }
    }

    @Override
    public boolean getLoadWithOverviewMode() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_LOAD_WITH_OVERVIEW_MODE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_LOAD_WITH_OVERVIEW_MODE);
            return mAwSettings.getLoadWithOverviewMode();
        }
    }

    @Override
    public void setSafeBrowsingEnabled(boolean accept) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_SAFE_BROWSING_ENABLED")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_SAFE_BROWSING_ENABLED);
            mAwSettings.setSafeBrowsingEnabled(accept);
        }
    }

    @Override
    public boolean getSafeBrowsingEnabled() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_SAFE_BROWSING_ENABLED")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_SAFE_BROWSING_ENABLED);
            return mAwSettings.getSafeBrowsingEnabled();
        }
    }

    @Override
    public void setAcceptThirdPartyCookies(boolean accept) {
        mAwSettings.setAcceptThirdPartyCookies(accept);
    }

    @Override
    public boolean getAcceptThirdPartyCookies() {
        return mAwSettings.getAcceptThirdPartyCookies();
    }

    @Override
    public void setEnableSmoothTransition(boolean enable) {
        // Intentional no-op.
    }

    @Override
    public boolean enableSmoothTransition() {
        // Intentional no-op.
        return false;
    }

    @Override
    public void setUseWebViewBackgroundForOverscrollBackground(boolean view) {
        // Intentional no-op.
    }

    @Override
    public boolean getUseWebViewBackgroundForOverscrollBackground() {
        // Intentional no-op.
        return false;
    }

    @Override
    public void setSaveFormData(boolean save) {
        // Intentional no-op since Android O.
    }

    @Override
    public boolean getSaveFormData() {
        // Intentional no-op since Android O.
        return false;
    }

    @Override
    public void setSavePassword(boolean save) {
        // Intentional no-op.
    }

    @Override
    public boolean getSavePassword() {
        // Intentional no-op.
        return false;
    }

    @Override
    public synchronized void setTextZoom(int textZoom) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_TEXT_ZOOM")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_TEXT_ZOOM);
            mAwSettings.setTextZoom(textZoom);
        }
    }

    @Override
    public synchronized int getTextZoom() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_GET_TEXT_ZOOM")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_TEXT_ZOOM);
            return mAwSettings.getTextZoom();
        }
    }

    @Override
    public void setDefaultZoom(ZoomDensity zoom) {
        // Intentional no-op
    }

    @Override
    public ZoomDensity getDefaultZoom() {
        // Intentional no-op
        return ZoomDensity.MEDIUM;
    }

    @Override
    public void setLightTouchEnabled(boolean enabled) {
        // Intentional no-op.
    }

    @Override
    public boolean getLightTouchEnabled() {
        // Intentional no-op.
        return false;
    }

    @Override
    public synchronized void setUserAgent(int ua) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_USER_AGENT")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_USER_AGENT);
            mAwSettings.setUserAgent(ua);
        }
    }

    @Override
    public synchronized int getUserAgent() {
        // Minimal implementation for backwards compatibility: just identifies default vs custom.
        return AwSettings.getDefaultUserAgent().equals(getUserAgentString()) ? 0 : -1;
    }

    @Override
    public synchronized void setUseWideViewPort(boolean use) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_USE_WIDE_VIEW_PORT")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_USE_WIDE_VIEW_PORT);
            mAwSettings.setUseWideViewPort(use);
        }
    }

    @Override
    public synchronized boolean getUseWideViewPort() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_USE_WIDE_VIEW_PORT")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_USE_WIDE_VIEW_PORT);
            return mAwSettings.getUseWideViewPort();
        }
    }

    @Override
    public synchronized void setSupportMultipleWindows(boolean support) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_SUPPORT_MULTIPLE_WINDOWS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_SUPPORT_MULTIPLE_WINDOWS);
            mAwSettings.setSupportMultipleWindows(support);
        }
    }

    @Override
    public synchronized boolean supportMultipleWindows() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SUPPORT_MULTIPLE_WINDOWS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SUPPORT_MULTIPLE_WINDOWS);
            return mAwSettings.supportMultipleWindows();
        }
    }

    @Override
    public synchronized void setLayoutAlgorithm(LayoutAlgorithm l) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_LAYOUT_ALGORITHM")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_LAYOUT_ALGORITHM);
            switch (l) {
                case NORMAL:
                    mAwSettings.setLayoutAlgorithm(AwSettings.LAYOUT_ALGORITHM_NORMAL);
                    return;
                case SINGLE_COLUMN:
                    mAwSettings.setLayoutAlgorithm(AwSettings.LAYOUT_ALGORITHM_SINGLE_COLUMN);
                    return;
                case NARROW_COLUMNS:
                    mAwSettings.setLayoutAlgorithm(AwSettings.LAYOUT_ALGORITHM_NARROW_COLUMNS);
                    return;
                case TEXT_AUTOSIZING:
                    mAwSettings.setLayoutAlgorithm(AwSettings.LAYOUT_ALGORITHM_TEXT_AUTOSIZING);
                    return;
                default:
                    throw new IllegalArgumentException("Unsupported value: " + l);
            }
        }
    }

    @Override
    public synchronized LayoutAlgorithm getLayoutAlgorithm() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_GET_LAYOUT_ALGORITHM")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_LAYOUT_ALGORITHM);
            int value = mAwSettings.getLayoutAlgorithm();
            switch (value) {
                case AwSettings.LAYOUT_ALGORITHM_NORMAL:
                    return LayoutAlgorithm.NORMAL;
                case AwSettings.LAYOUT_ALGORITHM_SINGLE_COLUMN:
                    return LayoutAlgorithm.SINGLE_COLUMN;
                case AwSettings.LAYOUT_ALGORITHM_NARROW_COLUMNS:
                    return LayoutAlgorithm.NARROW_COLUMNS;
                case AwSettings.LAYOUT_ALGORITHM_TEXT_AUTOSIZING:
                    return LayoutAlgorithm.TEXT_AUTOSIZING;
                default:
                    throw new IllegalArgumentException("Unsupported value: " + value);
            }
        }
    }

    @Override
    public synchronized void setStandardFontFamily(String font) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_STANDARD_FONT_FAMILY")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_STANDARD_FONT_FAMILY);
            mAwSettings.setStandardFontFamily(font);
        }
    }

    @Override
    public synchronized String getStandardFontFamily() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_STANDARD_FONT_FAMILY")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_STANDARD_FONT_FAMILY);
            return mAwSettings.getStandardFontFamily();
        }
    }

    @Override
    public synchronized void setFixedFontFamily(String font) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_FIXED_FONT_FAMILY")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_FIXED_FONT_FAMILY);
            mAwSettings.setFixedFontFamily(font);
        }
    }

    @Override
    public synchronized String getFixedFontFamily() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_GET_FIXED_FONT_FAMILY")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_FIXED_FONT_FAMILY);
            return mAwSettings.getFixedFontFamily();
        }
    }

    @Override
    public synchronized void setSansSerifFontFamily(String font) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_SANS_SERIF_FONT_FAMILY")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_SANS_SERIF_FONT_FAMILY);
            mAwSettings.setSansSerifFontFamily(font);
        }
    }

    @Override
    public synchronized String getSansSerifFontFamily() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_SANS_SERIF_FONT_FAMILY")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_SANS_SERIF_FONT_FAMILY);
            return mAwSettings.getSansSerifFontFamily();
        }
    }

    @Override
    public synchronized void setSerifFontFamily(String font) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_SERIF_FONT_FAMILY")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_SERIF_FONT_FAMILY);
            mAwSettings.setSerifFontFamily(font);
        }
    }

    @Override
    public synchronized String getSerifFontFamily() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_GET_SERIF_FONT_FAMILY")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_SERIF_FONT_FAMILY);
            return mAwSettings.getSerifFontFamily();
        }
    }

    @Override
    public synchronized void setCursiveFontFamily(String font) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_CURSIVE_FONT_FAMILY")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_CURSIVE_FONT_FAMILY);
            mAwSettings.setCursiveFontFamily(font);
        }
    }

    @Override
    public synchronized String getCursiveFontFamily() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_CURSIVE_FONT_FAMILY")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_CURSIVE_FONT_FAMILY);
            return mAwSettings.getCursiveFontFamily();
        }
    }

    @Override
    public synchronized void setFantasyFontFamily(String font) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_FANTASY_FONT_FAMILY")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_FANTASY_FONT_FAMILY);
            mAwSettings.setFantasyFontFamily(font);
        }
    }

    @Override
    public synchronized String getFantasyFontFamily() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_FANTASY_FONT_FAMILY")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_FANTASY_FONT_FAMILY);
            return mAwSettings.getFantasyFontFamily();
        }
    }

    @Override
    public synchronized void setMinimumFontSize(int size) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_MINIMUM_FONT_SIZE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_MINIMUM_FONT_SIZE);
            mAwSettings.setMinimumFontSize(size);
        }
    }

    @Override
    public synchronized int getMinimumFontSize() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_GET_MINIMUM_FONT_SIZE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_MINIMUM_FONT_SIZE);
            return mAwSettings.getMinimumFontSize();
        }
    }

    @Override
    public synchronized void setMinimumLogicalFontSize(int size) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_MINIMUM_LOGICAL_FONT_SIZE")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_SET_MINIMUM_LOGICAL_FONT_SIZE);
            mAwSettings.setMinimumLogicalFontSize(size);
        }
    }

    @Override
    public synchronized int getMinimumLogicalFontSize() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_MINIMUM_LOGICAL_FONT_SIZE")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_GET_MINIMUM_LOGICAL_FONT_SIZE);
            return mAwSettings.getMinimumLogicalFontSize();
        }
    }

    @Override
    public synchronized void setDefaultFontSize(int size) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_DEFAULT_FONT_SIZE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_DEFAULT_FONT_SIZE);
            mAwSettings.setDefaultFontSize(size);
        }
    }

    @Override
    public synchronized int getDefaultFontSize() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_DEFAULT_FIXED_FONT_SIZE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_DEFAULT_FIXED_FONT_SIZE);
            return mAwSettings.getDefaultFontSize();
        }
    }

    @Override
    public synchronized void setDefaultFixedFontSize(int size) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_DEFAULT_FIXED_FONT_SIZE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_DEFAULT_FIXED_FONT_SIZE);
            mAwSettings.setDefaultFixedFontSize(size);
        }
    }

    @Override
    public synchronized int getDefaultFixedFontSize() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_DEFAULT_FIXED_FONT_SIZE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_DEFAULT_FIXED_FONT_SIZE);
            return mAwSettings.getDefaultFixedFontSize();
        }
    }

    @Override
    public synchronized void setLoadsImagesAutomatically(boolean flag) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_LOADS_IMAGES_AUTOMATICALLY")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_SET_LOADS_IMAGES_AUTOMATICALLY);
            mAwSettings.setLoadsImagesAutomatically(flag);
        }
    }

    @Override
    public synchronized boolean getLoadsImagesAutomatically() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_LOADS_IMAGES_AUTOMATICALLY")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_GET_LOADS_IMAGES_AUTOMATICALLY);
            return mAwSettings.getLoadsImagesAutomatically();
        }
    }

    @Override
    public synchronized void setBlockNetworkImage(boolean flag) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_BLOCK_NETWORK_IMAGE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_BLOCK_NETWORK_IMAGE);
            mAwSettings.setImagesEnabled(!flag);
        }
    }

    @Override
    public synchronized boolean getBlockNetworkImage() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_BLOCK_NETWORK_IMAGE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_BLOCK_NETWORK_IMAGE);
            return !mAwSettings.getImagesEnabled();
        }
    }

    @Override
    public synchronized void setBlockNetworkLoads(boolean flag) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_BLOCK_NETWORK_LOADS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_BLOCK_NETWORK_LOADS);
            mAwSettings.setBlockNetworkLoads(flag);
        }
    }

    @Override
    public synchronized boolean getBlockNetworkLoads() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_BLOCK_NETWORK_LOADS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_BLOCK_NETWORK_LOADS);
            return mAwSettings.getBlockNetworkLoads();
        }
    }

    @Override
    public synchronized void setJavaScriptEnabled(boolean flag) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_JAVA_SCRIPT_ENABLED")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_JAVA_SCRIPT_ENABLED);
            mAwSettings.setJavaScriptEnabled(flag);
        }
    }

    @Override
    public void setAllowUniversalAccessFromFileURLs(boolean flag) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.ApiCall.WEB_SETTINGS_SET_ALLOW_UNIVERSAL_ACCESS_FROM_FILE_URLS")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_SET_ALLOW_UNIVERSAL_ACCESS_FROM_FILE_URLS);
            mAwSettings.setAllowUniversalAccessFromFileURLs(flag);
        }
    }

    @Override
    public void setAllowFileAccessFromFileURLs(boolean flag) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.ApiCall.WEB_SETTINGS_SET_ALLOW_FILE_ACCESS_FROM_FILE_URLS")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_SET_ALLOW_FILE_ACCESS_FROM_FILE_URLS);
            mAwSettings.setAllowFileAccessFromFileURLs(flag);
        }
    }

    @Override
    public synchronized void setPluginsEnabled(boolean flag) {
        mPluginState = flag ? PluginState.ON : PluginState.OFF;
    }

    @Override
    public synchronized void setPluginState(PluginState state) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_PLUGIN_STATE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_PLUGIN_STATE);
            mPluginState = state;
        }
    }

    @Override
    public synchronized void setDatabasePath(String databasePath) {
        // Intentional no-op.
    }

    @Override
    public synchronized void setGeolocationDatabasePath(String databasePath) {
        // Intentional no-op.
    }

    // removed from the public SDK in 33, so cannot @Override, but must remain
    // to not break apps compiled against previous SDKs.
    public synchronized void setAppCacheEnabled(boolean flag) {
        // Intentional no-op.
    }

    // removed from the public SDK in 33, so cannot @Override, but must remain
    // to not break apps compiled against previous SDKs.
    public synchronized void setAppCachePath(String appCachePath) {
        // Intentional no-op.
    }

    // removed from the public SDK in 33, so cannot @Override, but must remain
    // to not break apps compiled against previous SDKs.
    public synchronized void setAppCacheMaxSize(long appCacheMaxSize) {
        // Intentional no-op.
    }

    @Override
    public synchronized void setDatabaseEnabled(boolean flag) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_DATABASE_ENABLED")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_DATABASE_ENABLED);
            mAwSettings.setDatabaseEnabled(flag);
        }
    }

    @Override
    public synchronized void setDomStorageEnabled(boolean flag) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_DOM_STORAGE_ENABLED")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_DOM_STORAGE_ENABLED);
            mAwSettings.setDomStorageEnabled(flag);
        }
    }

    @Override
    public synchronized boolean getDomStorageEnabled() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_DOM_STORAGE_ENABLED")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_DOM_STORAGE_ENABLED);
            return mAwSettings.getDomStorageEnabled();
        }
    }

    @Override
    public synchronized String getDatabasePath() {
        // Intentional no-op.
        return "";
    }

    @Override
    public synchronized boolean getDatabaseEnabled() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_GET_DATABASE_ENABLED")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_DATABASE_ENABLED);
            return mAwSettings.getDatabaseEnabled();
        }
    }

    @Override
    public synchronized void setGeolocationEnabled(boolean flag) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_GEOLOCATION_ENABLED")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_GEOLOCATION_ENABLED);
            mAwSettings.setGeolocationEnabled(flag);
        }
    }

    @Override
    public synchronized boolean getJavaScriptEnabled() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_JAVA_SCRIPT_ENABLED")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_JAVA_SCRIPT_ENABLED);
            return mAwSettings.getJavaScriptEnabled();
        }
    }

    @Override
    public boolean getAllowUniversalAccessFromFileURLs() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.ApiCall.WEB_SETTINGS_GET_ALLOW_UNIVERSAL_ACCESS_FROM_FILE_URLS")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_GET_ALLOW_UNIVERSAL_ACCESS_FROM_FILE_URLS);
            return mAwSettings.getAllowUniversalAccessFromFileURLs();
        }
    }

    @Override
    public boolean getAllowFileAccessFromFileURLs() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.ApiCall.WEB_SETTINGS_GET_ALLOW_FILE_ACCESS_FROM_FILE_URLS")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_GET_ALLOW_FILE_ACCESS_FROM_FILE_URLS);
            return mAwSettings.getAllowFileAccessFromFileURLs();
        }
    }

    @Override
    public synchronized boolean getPluginsEnabled() {
        return mPluginState == PluginState.ON;
    }

    @Override
    public synchronized PluginState getPluginState() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_GET_PLUGIN_STATE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_PLUGIN_STATE);
            return mPluginState;
        }
    }

    @Override
    public synchronized void setJavaScriptCanOpenWindowsAutomatically(boolean flag) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.ApiCall.WEB_SETTINGS_SET_JAVA_SCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_SET_JAVA_SCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY);
            mAwSettings.setJavaScriptCanOpenWindowsAutomatically(flag);
        }
    }

    @Override
    public synchronized boolean getJavaScriptCanOpenWindowsAutomatically() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.ApiCall.WEB_SETTINGS_GET_JAVA_SCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_GET_JAVA_SCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY);
            return mAwSettings.getJavaScriptCanOpenWindowsAutomatically();
        }
    }

    @Override
    public synchronized void setDefaultTextEncodingName(String encoding) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_DEFAULT_TEXT_ENCODING_NAME")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_SET_DEFAULT_TEXT_ENCODING_NAME);
            mAwSettings.setDefaultTextEncodingName(encoding);
        }
    }

    @Override
    public synchronized String getDefaultTextEncodingName() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_DEFAULT_TEXT_ENCODING_NAME")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_GET_DEFAULT_TEXT_ENCODING_NAME);
            return mAwSettings.getDefaultTextEncodingName();
        }
    }

    @Override
    public synchronized void setUserAgentString(String ua) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_USER_AGENT_STRING")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_USER_AGENT_STRING);
            mAwSettings.setUserAgentString(ua);
        }
    }

    @Override
    public synchronized String getUserAgentString() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_GET_USER_AGENT_STRING")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_USER_AGENT_STRING);
            return mAwSettings.getUserAgentString();
        }
    }

    @Override
    public void setNeedInitialFocus(boolean flag) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_NEED_INITIAL_FOCUS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_NEED_INITIAL_FOCUS);
            mAwSettings.setShouldFocusFirstNode(flag);
        }
    }

    @Override
    public synchronized void setRenderPriority(RenderPriority priority) {
        // Intentional no-op.
    }

    @Override
    public void setCacheMode(int mode) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_CACHE_MODE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_CACHE_MODE);
            mAwSettings.setCacheMode(mode);
        }
    }

    @Override
    public int getCacheMode() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_GET_CACHE_MODE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_CACHE_MODE);
            return mAwSettings.getCacheMode();
        }
    }

    @Override
    public void setMediaPlaybackRequiresUserGesture(boolean require) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.ApiCall.WEB_SETTINGS_SET_MEDIA_PLAYBACK_REQUIRES_USER_GESTURE")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_SET_MEDIA_PLAYBACK_REQUIRES_USER_GESTURE);
            mAwSettings.setMediaPlaybackRequiresUserGesture(require);
        }
    }

    @Override
    public boolean getMediaPlaybackRequiresUserGesture() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.ApiCall.WEB_SETTINGS_GET_MEDIA_PLAYBACK_REQUIRES_USER_GESTURE")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_GET_MEDIA_PLAYBACK_REQUIRES_USER_GESTURE);
            return mAwSettings.getMediaPlaybackRequiresUserGesture();
        }
    }

    @Override
    public void setMixedContentMode(int mode) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_MIXED_CONTENT_MODE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_MIXED_CONTENT_MODE);
            mAwSettings.setMixedContentMode(mode);
        }
    }

    @Override
    public int getMixedContentMode() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_MIXED_CONTENT_MODE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_MIXED_CONTENT_MODE);
            return mAwSettings.getMixedContentMode();
        }
    }

    @Override
    public void setOffscreenPreRaster(boolean enabled) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_OFFSCREEN_PRE_RASTER")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_OFFSCREEN_PRE_RASTER);
            mAwSettings.setOffscreenPreRaster(enabled);
        }
    }

    @Override
    public boolean getOffscreenPreRaster() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_GET_OFFSCREEN_PRE_RASTER")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_OFFSCREEN_PRE_RASTER);
            return mAwSettings.getOffscreenPreRaster();
        }
    }

    @Override
    public void setDisabledActionModeMenuItems(int menuItems) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.ApiCall.WEB_SETTINGS_SET_DISABLED_ACTION_MODE_MENU_ITEMS")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_SET_DISABLED_ACTION_MODE_MENU_ITEMS);
            mAwSettings.setDisabledActionModeMenuItems(menuItems);
        }
    }

    @Override
    public int getDisabledActionModeMenuItems() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.ApiCall.WEB_SETTINGS_GET_DISABLED_ACTION_MODE_MENU_ITEMS")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_GET_DISABLED_ACTION_MODE_MENU_ITEMS);
            return mAwSettings.getDisabledActionModeMenuItems();
        }
    }

    @Override
    public void setVideoOverlayForEmbeddedEncryptedVideoEnabled(boolean flag) {
        // No-op, see http://crbug.com/616583
    }

    @Override
    public boolean getVideoOverlayForEmbeddedEncryptedVideoEnabled() {
        // Always false, see http://crbug.com/616583
        return false;
    }

    @Override
    @SuppressLint("Override")
    public void setForceDark(int forceDarkMode) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_SET_FORCE_DARK")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_SET_FORCE_DARK);
            if (AwDarkMode.isSimplifiedDarkModeEnabled()) {
                Log.w(TAG, "setForceDark() is a no-op in an app with targetSdkVersion>=T");
                return;
            }
            switch (forceDarkMode) {
                case WebSettings.FORCE_DARK_OFF:
                    getAwSettings().setForceDarkMode(AwSettings.FORCE_DARK_OFF);
                    break;
                case WebSettings.FORCE_DARK_AUTO:
                    getAwSettings().setForceDarkMode(AwSettings.FORCE_DARK_AUTO);
                    break;
                case WebSettings.FORCE_DARK_ON:
                    getAwSettings().setForceDarkMode(AwSettings.FORCE_DARK_ON);
                    break;
                default:
                    throw new IllegalArgumentException(
                            "Force dark mode is not one of FORCE_DARK_(ON|OFF|AUTO)");
            }
        }
    }

    @Override
    @SuppressLint("Override")
    public int getForceDark() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.WEB_SETTINGS_GET_FORCE_DARK")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_SETTINGS_GET_FORCE_DARK);
            if (AwDarkMode.isSimplifiedDarkModeEnabled()) {
                Log.w(TAG, "getForceDark() is a no-op in an app with targetSdkVersion>=T");
                return WebSettings.FORCE_DARK_AUTO;
            }
            switch (getAwSettings().getForceDarkMode()) {
                case AwSettings.FORCE_DARK_OFF:
                    return WebSettings.FORCE_DARK_OFF;
                case AwSettings.FORCE_DARK_AUTO:
                    return WebSettings.FORCE_DARK_AUTO;
                case AwSettings.FORCE_DARK_ON:
                    return WebSettings.FORCE_DARK_ON;
            }
            return WebSettings.FORCE_DARK_AUTO;
        }
    }

    // Lint thinks we are targeting 32 so complains that this only overrides in 33, suppress until
    // b/239952654 is resolved
    @SuppressWarnings("Override")
    @Override
    public void setAlgorithmicDarkeningAllowed(boolean allow) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_SET_ALGORITHMIC_DARKENING_ALLOWED")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_SET_ALGORITHMIC_DARKENING_ALLOWED);
            if (!AwDarkMode.isSimplifiedDarkModeEnabled()) {
                Log.w(
                        TAG,
                        "setAlgorithmicDarkeningAllowed() is a no-op in an app with "
                                + "targetSdkVersion<T");
                return;
            }
            getAwSettings().setAlgorithmicDarkeningAllowed(allow);
        }
    }

    // Lint thinks we are targeting 32 so complains that this only overrides in 33, suppress until
    // b/239952654 is resolved
    @SuppressWarnings("Override")
    @Override
    public boolean isAlgorithmicDarkeningAllowed() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.WEB_SETTINGS_IS_ALGORITHMIC_DARKENING_ALLOWED")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.WEB_SETTINGS_IS_ALGORITHMIC_DARKENING_ALLOWED);
            if (!AwDarkMode.isSimplifiedDarkModeEnabled()) {
                Log.w(
                        TAG,
                        "isAlgorithmicDarkeningAllowed() is a no-op in an app with "
                                + "targetSdkVersion<T");
                return false;
            }
            return getAwSettings().isAlgorithmicDarkeningAllowed();
        }
    }
}
