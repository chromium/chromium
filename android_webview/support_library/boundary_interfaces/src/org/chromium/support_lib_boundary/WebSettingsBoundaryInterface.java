// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

// Technically this interface is not needed until we add a method to WebSettings with an
// android.webkit parameter or android.webkit return value. But for forwards compatibility all
// app-facing classes should have a boundary-interface that the WebView glue layer can build
// against.

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Map;
import java.util.Set;

/** Boundary interface for WebSettingsCompat. */
public interface WebSettingsBoundaryInterface {
    void setOffscreenPreRaster(boolean enabled);

    boolean getOffscreenPreRaster();

    void setSafeBrowsingEnabled(boolean enabled);

    boolean getSafeBrowsingEnabled();

    void setDisabledActionModeMenuItems(int menuItems);

    int getDisabledActionModeMenuItems();

    void setWillSuppressErrorPage(boolean suppressed);

    boolean getWillSuppressErrorPage();

    void setForceDark(int forceDarkMode);

    int getForceDark();

    void setAlgorithmicDarkeningAllowed(boolean allow);

    boolean isAlgorithmicDarkeningAllowed();

    @Retention(RetentionPolicy.SOURCE)
    @interface ForceDarkBehavior {
        int FORCE_DARK_ONLY = 0;
        int MEDIA_QUERY_ONLY = 1;
        int PREFER_MEDIA_QUERY_OVER_FORCE_DARK = 2;
    }

    void setForceDarkBehavior(@ForceDarkBehavior int forceDarkBehavior);

    @ForceDarkBehavior
    int getForceDarkBehavior();

    @Retention(RetentionPolicy.SOURCE)
    @interface WebauthnSupport {
        int NONE = 0;
        int APP = 1;
        int BROWSER = 2;
    }

    void setWebauthnSupport(@WebauthnSupport int support);

    @WebauthnSupport
    int getWebauthnSupport();

    void setRequestedWithHeaderOriginAllowList(Set<String> allowedOriginRules);

    Set<String> getRequestedWithHeaderOriginAllowList();

    void setEnterpriseAuthenticationAppLinkPolicyEnabled(boolean enabled);

    boolean getEnterpriseAuthenticationAppLinkPolicyEnabled();

    void setUserAgentMetadataFromMap(Map<String, Object> uaMetadata);

    Map<String, Object> getUserAgentMetadataMap();

    @Retention(RetentionPolicy.SOURCE)
    @interface AttributionBehavior {
        int DISABLED = 0;
        int APP_SOURCE_AND_WEB_TRIGGER = 1;
        int WEB_SOURCE_AND_WEB_TRIGGER = 2;
        int APP_SOURCE_AND_APP_TRIGGER = 3;
    }

    void setAttributionBehavior(@AttributionBehavior int behavior);

    @AttributionBehavior
    int getAttributionBehavior();

    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface WebViewMediaIntegrityApiStatus {
        int DISABLED = 0;
        int ENABLED_WITHOUT_APP_IDENTITY = 1;
        int ENABLED = 2;
    }

    void setWebViewMediaIntegrityApiStatus(
            @WebViewMediaIntegrityApiStatus int defaultPermission,
            Map<String, @WebViewMediaIntegrityApiStatus Integer> permissionConfig);

    @WebViewMediaIntegrityApiStatus
    int getWebViewMediaIntegrityApiDefaultStatus();

    Map<String, @WebViewMediaIntegrityApiStatus Integer> getWebViewMediaIntegrityApiOverrideRules();

    @Retention(RetentionPolicy.SOURCE)
    @interface SpeculativeLoadingStatus {
        int DISABLED = 0;
        int PRERENDER_ENABLED = 1;
    }

    void setSpeculativeLoadingStatus(@SpeculativeLoadingStatus int speculativeLoadingStatus);

    @SpeculativeLoadingStatus
    int getSpeculativeLoadingStatus();

    void setBackForwardCacheEnabled(boolean backForwardCacheEnabled);

    boolean getBackForwardCacheEnabled();
}
