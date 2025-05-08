// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.webkit.WebSettings;

import org.chromium.support_lib_boundary.WebSettingsBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.util.Collections;
import java.util.Map;
import java.util.Set;

/**
 * Mock adapter for WebSettings that doesn't do anything.
 *
 * <p>This class is used as a return value in the rare instances when the WebSettings object being
 * passed into the support lib layer is not an implementation from Chromium, and therefore not
 * castable to the implementation class.
 */
class SupportLibWebSettingsNoOpAdapter implements WebSettingsBoundaryInterface {

    public SupportLibWebSettingsNoOpAdapter() {}

    @Override
    public void setOffscreenPreRaster(boolean enabled) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_OFFSCREEN_PRE_RASTER);
    }

    @Override
    public boolean getOffscreenPreRaster() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_OFFSCREEN_PRE_RASTER);
        return false;
    }

    @Override
    public void setSafeBrowsingEnabled(boolean enabled) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_SAFE_BROWSING_ENABLED);
    }

    @Override
    public boolean getSafeBrowsingEnabled() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_SAFE_BROWSING_ENABLED);
        return true;
    }

    @Override
    public void setDisabledActionModeMenuItems(int menuItems) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_DISABLED_ACTION_MODE_MENU_ITEMS);
    }

    @Override
    public int getDisabledActionModeMenuItems() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_DISABLED_ACTION_MODE_MENU_ITEMS);
        return 0;
    }

    @Override
    public boolean getWillSuppressErrorPage() {
        return false;
    }

    @Override
    public void setWillSuppressErrorPage(boolean suppressed) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_WILL_SUPPRESS_ERROR_PAGE);
    }

    @Override
    public void setForceDark(int forceDarkMode) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_FORCE_DARK);
    }

    @Override
    public int getForceDark() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_FORCE_DARK);
        return WebSettings.FORCE_DARK_AUTO;
    }

    @Override
    public void setForceDarkBehavior(int forceDarkBehavior) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_FORCE_DARK_BEHAVIOR);
    }

    @Override
    public int getForceDarkBehavior() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_FORCE_DARK_BEHAVIOR);
        return ForceDarkBehavior.PREFER_MEDIA_QUERY_OVER_FORCE_DARK;
    }

    @Override
    public void setAlgorithmicDarkeningAllowed(boolean allow) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_ALGORITHMIC_DARKENING_ALLOWED);
    }

    @Override
    public boolean isAlgorithmicDarkeningAllowed() {
        recordApiCall(ApiCall.WEB_SETTINGS_IS_ALGORITHMIC_DARKENING_ALLOWED);
        return false;
    }

    @Override
    public void setWebauthnSupport(@WebauthnSupport int support) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_WEBAUTHN_SUPPORT);
    }

    @Override
    public @WebauthnSupport int getWebauthnSupport() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_WEBAUTHN_SUPPORT);
        return WebauthnSupport.NONE;
    }

    @Override
    public void setRequestedWithHeaderOriginAllowList(Set<String> allowedOriginRules) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST);
    }

    @Override
    public Set<String> getRequestedWithHeaderOriginAllowList() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST);
        return Collections.emptySet();
    }

    @Override
    public void setEnterpriseAuthenticationAppLinkPolicyEnabled(boolean enabled) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_ENABLED);
    }

    @Override
    public boolean getEnterpriseAuthenticationAppLinkPolicyEnabled() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_ENABLED);
        return false;
    }

    @Override
    public void setUserAgentMetadataFromMap(Map<String, Object> uaMetadata) {
        recordApiCall(ApiCall.WEB_SETTINGS_SET_USER_AGENT_METADATA);
    }

    @Override
    public Map<String, Object> getUserAgentMetadataMap() {
        recordApiCall(ApiCall.WEB_SETTINGS_GET_USER_AGENT_METADATA);
        return Collections.emptyMap();
    }

    @Override
    public void setAttributionBehavior(@AttributionBehavior int behavior) {
        recordApiCall(ApiCall.SET_ATTRIBUTION_BEHAVIOR);
    }

    @Override
    public int getAttributionBehavior() {
        recordApiCall(ApiCall.GET_ATTRIBUTION_BEHAVIOR);
        return AttributionBehavior.APP_SOURCE_AND_WEB_TRIGGER;
    }

    @Override
    public void setWebViewMediaIntegrityApiStatus(
            @WebViewMediaIntegrityApiStatus int defaultStatus,
            Map<String, @WebViewMediaIntegrityApiStatus Integer> permissionConfig) {
        recordApiCall(ApiCall.SET_WEBVIEW_MEDIA_INTEGRITY_API_STATUS);
    }

    @Override
    public @WebViewMediaIntegrityApiStatus int getWebViewMediaIntegrityApiDefaultStatus() {
        recordApiCall(ApiCall.GET_WEBVIEW_MEDIA_INTEGRITY_API_DEFAULT_STATUS);
        return WebViewMediaIntegrityApiStatus.ENABLED;
    }

    @Override
    public Map<String, @WebViewMediaIntegrityApiStatus Integer>
            getWebViewMediaIntegrityApiOverrideRules() {
        recordApiCall(ApiCall.GET_WEBVIEW_MEDIA_INTEGRITY_API_OVERRIDE_RULES);
        return Collections.emptyMap();
    }

    @Override
    public void setSpeculativeLoadingStatus(
            @SpeculativeLoadingStatus int speculativeLoadingStatus) {
        recordApiCall(ApiCall.SET_SPECULATIVE_LOADING_STATUS);
    }

    @Override
    public @SpeculativeLoadingStatus int getSpeculativeLoadingStatus() {
        recordApiCall(ApiCall.GET_SPECULATIVE_LOADING_STATUS);
        return SpeculativeLoadingStatus.DISABLED;
    }

    @Override
    public void setBackForwardCacheEnabled(boolean backForwardCacheEnabled) {
        recordApiCall(ApiCall.SET_BACK_FORWARD_CACHE_ENABLED);
    }

    @Override
    public boolean getBackForwardCacheEnabled() {
        recordApiCall(ApiCall.GET_BACK_FORWARD_CACHE_ENABLED);
        return false;
    }

    @Override
    public void setPaymentRequestEnabled(boolean enabled) {
        recordApiCall(ApiCall.SET_PAYMENT_REQUEST_ENABLED);
    }

    @Override
    public boolean getPaymentRequestEnabled() {
        recordApiCall(ApiCall.GET_PAYMENT_REQUEST_ENABLED);
        return false;
    }

    @Override
    public void setHasEnrolledInstrumentEnabled(boolean enabled) {
        recordApiCall(ApiCall.SET_HAS_ENROLLED_INSTRUMENT_ENABLED);
    }

    @Override
    public boolean getHasEnrolledInstrumentEnabled() {
        recordApiCall(ApiCall.GET_HAS_ENROLLED_INSTRUMENT_ENABLED);
        return false;
    }

    @Override
    public void setIncludeCookiesOnIntercept(boolean includeCookiesOnIntercept) {
        recordApiCall(ApiCall.SET_INCLUDE_COOKIES_ON_INTERCEPT);
    }

    @Override
    public boolean getIncludeCookiesOnIntercept() {
        recordApiCall(ApiCall.GET_INCLUDE_COOKIES_ON_INTERCEPT);
        return false;
    }
}
