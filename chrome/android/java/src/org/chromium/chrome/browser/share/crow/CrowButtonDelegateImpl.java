// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.crow;

import android.content.Context;
import android.net.Uri;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.HashMap;

/** Implementation of Crow share chip-related actions. */
public class CrowButtonDelegateImpl implements CrowButtonDelegate {
    /** Domain to ID map, populated on first read. */
    private HashMap<String, String> mDomainIdMap;

    private static final String APP_MENU_BUTTON_TEXT_PARAM = "AppMenuButtonText";
    private static final String DEBUG_SERVER_URL_PARAM = "DebugServerURL";
    private static final String DOMAIN_LIST_URL_PARAM = "DomainList";
    private static final String DOMAIN_ID_NONE = "0";
    private static final String DEFAULT_BUTTON_TEXT = "Thank\u00A0creator";

    private static final String TAG = "CrowButton";

    /** Constructs a new {@link CrowButtonDelegateImpl}. */
    public CrowButtonDelegateImpl() {}

    @Override
    public boolean isEnabledForSite(GURL url) {
        // TODO(skare): Make this an AMP-aware comparison if needed.
        return isCrowEnabled() && !getPublicationId(url).equals(DOMAIN_ID_NONE);
    }

    @Override
    public void launchCustomTab(
            Context currentContext, GURL pageUrl, GURL canonicalUrl, boolean isFollowing) {
        String customTabUrl = buildServerUrl(new GURL(getServerUrl()), pageUrl, canonicalUrl,
                getPublicationId(pageUrl), areMetricsEnabled(), isFollowing);

        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setColorScheme(ColorUtils.inNightMode(currentContext)
                        ? CustomTabsIntent.COLOR_SCHEME_DARK
                        : CustomTabsIntent.COLOR_SCHEME_LIGHT);
        builder.setShareState(CustomTabsIntent.SHARE_STATE_OFF);
        CustomTabsIntent customTabsIntent = builder.build();
        customTabsIntent.intent.setClassName(currentContext, CustomTabActivity.class.getName());
        customTabsIntent.launchUrl(currentContext, Uri.parse(customTabUrl));
    }

    public boolean isCrowEnabled() {
        return ChromeFeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.SHARE_CROW_BUTTON);
    }

    /**
     * Parses a study-provided list of enabled domains of the form
     * domain1^id1;domain2^id2;domain3^d3
     * Separators were chosen as they do not need to be URL-escaped in the Finch config,
     * improving readability, as the domains and debug server URL _will_ have URL-encoded
     * characters. Furthermore, study param-based configuration will go away during
     * feature development.
     *
     * @param Domains study parameter in serialized string form.
     */
    @VisibleForTesting
    public HashMap<String, String> parseDomainIdMap(String mapStudyParam) {
        HashMap<String, String> map = new HashMap<String, String>();
        // Limit to 20 domains during feature development.
        String[] allowlistEntries = mapStudyParam.split("\\;", 20);
        for (String entry : allowlistEntries) {
            String[] pair = entry.split("\\^", 2);
            if (pair.length < 2) break;
            map.put(pair[0], pair[1]);
        }
        return map;
    }

    private String getPublicationId(GURL url) {
        String host = url.getHost();

        // First check the downloaded component.
        String publicationID = CrowBridge.getPublicationIDForHost(host);
        if (!publicationID.isEmpty()) {
            return publicationID;
        }

        // Then check the experimental Finch config.
        if (mDomainIdMap == null) {
            mDomainIdMap = parseDomainIdMap(ChromeFeatureList.getFieldTrialParamByFeature(
                    ChromeFeatureList.SHARE_CROW_BUTTON, DOMAIN_LIST_URL_PARAM));
        }
        if (!mDomainIdMap.containsKey(host)) {
            return DOMAIN_ID_NONE;
        }
        return mDomainIdMap.get(host);
    }

    @Override
    public String getButtonText() {
        String param = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.SHARE_CROW_BUTTON, APP_MENU_BUTTON_TEXT_PARAM);
        // Provide a default with non-breaking space. String is en-us only.
        if (param.isEmpty()) {
            return DEFAULT_BUTTON_TEXT;
        }
        return param;
    }

    @Override
    public void requestCanonicalUrl(Tab tab, Callback<GURL> callback) {
        if (tab.getWebContents() == null || tab.getWebContents().getMainFrame() == null
                || tab.getUrl().isEmpty()) {
            callback.onResult(GURL.emptyGURL());
            return;
        }
        tab.getWebContents().getMainFrame().getCanonicalUrlForSharing(callback);
    }

    private String getServerUrl() {
        return ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.SHARE_CROW_BUTTON, DEBUG_SERVER_URL_PARAM);
    }

    private boolean areMetricsEnabled() {
        // Require UMA and "Make searches and browsing better" to be enabled.
        return (PrivacyPreferencesManagerImpl.getInstance().isUsageAndCrashReportingPermitted()
                && UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                        Profile.getLastUsedRegularProfile()));
    }

    @VisibleForTesting
    public String buildServerUrl(GURL serverUrl, GURL pageUrl, GURL canonicalPageUrl,
            String publicationId, boolean allowMetrics, boolean isFollowing) {
        String serverSpec = serverUrl.getSpec();
        if (serverSpec.isEmpty()) return "";
        Uri.Builder builder = Uri.parse(serverSpec).buildUpon();
        builder.appendQueryParameter("pageUrl", pageUrl.getSpec());
        builder.appendQueryParameter("entry", "menu");
        builder.appendQueryParameter("relCanonUrl", canonicalPageUrl.getSpec());
        builder.appendQueryParameter("publicationId", publicationId);
        builder.appendQueryParameter("metrics", allowMetrics ? "true" : "false");
        if (isFollowing) {
            builder.appendQueryParameter("following", "true");
        }
        return builder.build().toString();
    }
}
