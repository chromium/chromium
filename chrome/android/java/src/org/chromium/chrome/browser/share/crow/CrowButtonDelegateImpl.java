// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.crow;

import android.content.Context;
import android.net.Uri;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Callback;
import org.chromium.base.LocaleUtils;
import org.chromium.chrome.browser.ChromeActivitySessionTracker;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashMap;

/** Implementation of Crow share chip-related actions. */
public class CrowButtonDelegateImpl implements CrowButtonDelegate {
    /** Domain to ID map, populated on first read. */
    private HashMap<String, String> mDomainIdMap;
    // Tracker used to get the latest country of the user.
    private final ChromeActivitySessionTracker mChromeActivitySessionTracker;

    private static final String APP_MENU_BUTTON_TEXT_PARAM = "AppMenuButtonText";
    private static final String DEBUG_SERVER_URL_PARAM = "DebugServerURL";
    private static final String DOMAIN_LIST_URL_PARAM = "DomainList";
    private static final String DOMAIN_ID_NONE = "0";
    private static final String DEFAULT_BUTTON_TEXT = "Thank\u00A0creator";
    private static final String USE_PAGE_OPTIMIZATIONS_STUDY_PARAM = "UsePageOptimizations";

    private static final String TAG = "CrowButton";

    private static final MutableFlagWithSafeDefault sShareCrowButton =
            new MutableFlagWithSafeDefault(ChromeFeatureList.SHARE_CROW_BUTTON, false);
    private static final MutableFlagWithSafeDefault sShareCrowButtonLaunchTabFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.SHARE_CROW_BUTTON_LAUNCH_TAB, false);

    /** Constructs a new {@link CrowButtonDelegateImpl}. */
    public CrowButtonDelegateImpl() {
        mChromeActivitySessionTracker = ChromeActivitySessionTracker.getInstance();
    }

    // Lazy initialization of OptimizationGuideBridgeFactory
    private static class OptimizationGuideBridgeFactoryHolder {
        private static final OptimizationGuideBridgeFactory sOptimizationGuideBridgeFactory;
        static {
            sOptimizationGuideBridgeFactory = new OptimizationGuideBridgeFactory(
                    Arrays.asList(HintsProto.OptimizationType.THANK_CREATOR_ELIGIBLE));
        }
    }

    @Override
    public void isEnabledForSite(GURL url, Callback<Boolean> callback) {
        // TODO(skare): Make this an AMP-aware comparison if needed.
        if (!isCrowEnabled()) {
            callback.onResult(false);
            return;
        }

        // Any host present in the denylist should not have the feature enabled.
        if (CrowBridge.denylistContainsHost(url.getHost())) {
            callback.onResult(false);
            return;
        }

        // Check for an exact match against our allowlist.
        // Fall back to page optimizations if the study param is enabled.
        if (!getPublicationId(url).equals(DOMAIN_ID_NONE)) {
            callback.onResult(true);
            return;
        }
        if (!ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.SHARE_CROW_BUTTON, USE_PAGE_OPTIMIZATIONS_STUDY_PARAM,
                    false)) {
            callback.onResult(false);
            return;
        }
        checkPageOptimizations(url, callback);
    }

    private static void checkPageOptimizations(GURL url, Callback<Boolean> callback) {
        OptimizationGuideBridgeFactoryHolder.sOptimizationGuideBridgeFactory.create()
                .canApplyOptimization(url, HintsProto.OptimizationType.THANK_CREATOR_ELIGIBLE,
                        (decision, metadata) -> {
                            callback.onResult(decision == OptimizationGuideDecision.TRUE);
                        });
    }

    @Override
    public void launchCustomTab(
            Tab tab, Context currentContext, GURL pageUrl, GURL canonicalUrl, boolean isFollowing) {
        String customTabUrl = getUrlForWebFlow(pageUrl, canonicalUrl, isFollowing);

        // Experiment flag: open in standard new tab.
        if (sShareCrowButtonLaunchTabFlag.isEnabled()) {
            // TabLaunchType.FROM_LINK will allow back to navigate back to the
            // current tab.
            LoadUrlParams loadUrlParams = new LoadUrlParams(customTabUrl);
            new TabDelegate(false).createNewTab(loadUrlParams, TabLaunchType.FROM_LINK, tab);
        } else {
            // Default to custom tab.
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
    }

    /**
     * Returns true if the user is in the US using en-US, and false otherwise.
     *
     * <p>This should match how Finch gates features by locale and country. See
     * VariationsService::GetLatestCountry() and language::GetApplicationLocale().
     */
    private boolean isEnabledForLocaleAndCountry() {
        String country = mChromeActivitySessionTracker.getVariationsLatestCountry();
        String locale = AppLocaleUtils.getAppLanguagePref();
        if (locale == null) {
            locale = LocaleUtils.getDefaultLocaleString();
        }
        return country != null && country.equals("us") && locale.equals("en-US");
    }

    public boolean isCrowEnabled() {
        return isEnabledForLocaleAndCountry() && sShareCrowButton.isEnabled();
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

        // Check the allowlist for an exact match.
        String publicationID = CrowBridge.getPublicationIDFromAllowlist(host);
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

    @Override
    public String getUrlForWebFlow(GURL pageUrl, GURL canonicalPageUrl, boolean isFollowing) {
        return buildServerUrlInternal(new GURL(getServerUrl()), pageUrl, canonicalPageUrl,
                getPublicationId(pageUrl), areMetricsEnabled(), isFollowing);
    }

    @VisibleForTesting
    public String buildServerUrlInternal(GURL serverUrl, GURL pageUrl, GURL canonicalPageUrl,
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
