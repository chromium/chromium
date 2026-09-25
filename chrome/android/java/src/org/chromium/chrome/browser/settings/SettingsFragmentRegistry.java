// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.net.Uri;
import android.os.Bundle;
import android.util.ArrayMap;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import com.google.errorprone.annotations.CanIgnoreReturnValue;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.about_settings.LegalInformationSettings;
import org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment;
import org.chromium.chrome.browser.appearance.settings.BookmarkBarSettingsFragment;
import org.chromium.chrome.browser.autofill.settings.AndroidPaymentAppsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillAndPasswordsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillAndPasswordsFragment.AutofillSettingsReferrer;
import org.chromium.chrome.browser.autofill.settings.AutofillBuyNowPayLaterFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillCardBenefitsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillIdentityDocsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillProfilesFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillShoppingFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillTravelFragment;
import org.chromium.chrome.browser.autofill.settings.FinancialAccountsManagementFragment;
import org.chromium.chrome.browser.autofill.settings.NonCardPaymentMethodsManagementFragment;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsReferrer;
import org.chromium.chrome.browser.autofill.settings.personal_context.AutofillPersonalContextFragment;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.commerce.PriceNotificationSettingsFragment;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSettingsFragment;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
import org.chromium.chrome.browser.glic.GlicActorLoginPermissionsFragment;
import org.chromium.chrome.browser.glic.GlicSettings;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsSettings;
import org.chromium.chrome.browser.language.settings.LanguageSettings;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.ExtendedPreloadingSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.StandardPreloadingSettingsFragment;
import org.chromium.chrome.browser.privacy.secure_dns.SecureDnsSettings;
import org.chromium.chrome.browser.privacy.settings.DoNotTrackSettings;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.privacy.settings.UniversalOptOutSettings;
import org.chromium.chrome.browser.privacy_guide.PrivacyGuideFragment;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.EnhancedProtectionSettingsFragment;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safe_browsing.settings.StandardProtectionSettingsFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubNotificationsFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubPermissionsFragment;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.search_engines.settings.SiteSearchSettings;
import org.chromium.chrome.browser.ssl.HttpsFirstModeSettingsFragment;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.PersonalizeGoogleServicesSettings;
import org.chromium.chrome.browser.tasks.tab_management.TabsSettings;
import org.chromium.chrome.browser.tasks.tab_management.archived_tabs.TabArchiveSettingsFragment;
import org.chromium.chrome.browser.toolbar.adaptive.settings.AdaptiveToolbarSettingsFragment;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.browser.tracing.settings.TracingCategoriesSettings;
import org.chromium.chrome.browser.tracing.settings.TracingSettings;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.ChosenObjectSettings;
import org.chromium.components.browser_ui.site_settings.CookieSettings;
import org.chromium.components.browser_ui.site_settings.GroupedWebsitesSettings;
import org.chromium.components.browser_ui.site_settings.LocationPermissionSubpageSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.StorageAccessSubpageSettings;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsiteGroup;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.function.Consumer;

/** Centralized registry mapping chrome://settings URLs to Fragment classes. */
@NullMarked
public class SettingsFragmentRegistry {
    // Path maps are initialized statically to ensure constant-time URL routing.
    @VisibleForTesting
    static final ArrayMap<Class<? extends Fragment>, String> sFragmentToPathMap = new ArrayMap<>();

    @VisibleForTesting
    static final ArrayMap<String, String> sQueryParamToArgKeyMap = new ArrayMap<>();

    @VisibleForTesting
    static final ArrayMap<String, String> sArgKeyToQueryParamMap = new ArrayMap<>();

    @FunctionalInterface
    interface ParameterValueParser {
        void putValue(Bundle bundle, String argKey, String val);
    }

    @VisibleForTesting
    static final ArrayMap<String, ParameterValueParser> sQueryParamParsers = new ArrayMap<>();

    /**
     * Declarative rules for a registered route.
     *
     * <p>A settings URL is user editable and is replayed from browser history long after it was
     * created, so it cannot be assumed to carry the arguments its page needs: the user may have
     * trimmed a query parameter, or the data the parameter referred to may have been deleted.
     * Rather than letting each page assert its way to a crash, a route declares what it needs and
     * where to send the user when it is not there.
     */
    public static final class RouteSpec {
        private final Class<? extends Fragment> mFragmentClass;
        private String[] mRequiredArgKeys = new String[0];
        private @Nullable Consumer<Bundle> mDefaultsProvider;
        private @Nullable ArgValidator mValidator;
        private @Nullable String mFallbackPath;

        private RouteSpec(Class<? extends Fragment> fragmentClass) {
            mFragmentClass = fragmentClass;
        }

        /** Declares argument keys without which the page cannot be shown. */
        RouteSpec requireArgs(String... bundleKeys) {
            mRequiredArgKeys = bundleKeys;
            return this;
        }

        /** Declares arguments to fill in when the URL omits them. */
        RouteSpec withDefaults(Consumer<Bundle> provider) {
            mDefaultsProvider = provider;
            return this;
        }

        /** Declares a check on argument values, beyond their mere presence. */
        RouteSpec validateWith(ArgValidator validator) {
            mValidator = validator;
            return this;
        }

        /**
         * Declares where to send the user when a required argument is missing. Defaults to the
         * settings root.
         */
        RouteSpec fallback(String path) {
            mFallbackPath = path;
            return this;
        }
    }

    /** Checks argument values for a route. */
    @FunctionalInterface
    interface ArgValidator {
        /**
         * Returns the URL to redirect to instead of showing the page, or null if the arguments are
         * usable as they are.
         */
        @Nullable String validate(Bundle args);
    }

    /** The outcome of resolving a settings URL. */
    public static final class Resolution {
        /** The page to show. Null only when {@link #redirectUrl} is set. */
        public final @Nullable Class<? extends Fragment> fragmentClass;

        /** Arguments for {@link #fragmentClass}. */
        public final Bundle args;

        /** A URL to navigate to instead of showing a page, or null to show the page. */
        public final @Nullable String redirectUrl;

        private Resolution(
                @Nullable Class<? extends Fragment> fragmentClass,
                Bundle args,
                @Nullable String redirectUrl) {
            this.fragmentClass = fragmentClass;
            this.args = args;
            this.redirectUrl = redirectUrl;
        }
    }

    @VisibleForTesting
    static final ArrayMap<String, RouteSpec> sPathToRouteSpecMap = new ArrayMap<>();

    /**
     * Used for highlighting fragments that aren't mapped to getMainMenuKey() (some subpages).
     *
     * <p>Deliberately distinct from EmbeddableSettingsPage.getMainMenuKey(), which declares that a
     * fragment is a given main menu row. That is an identity and must stay globally unique, because
     * the settings search index keys its entries by preference key.
     *
     * <p>Only needed for pages that are absent from the search index, i.e. those attached at
     * runtime rather than declared with in a preference XML.
     */
    @VisibleForTesting
    static final ArrayMap<Class<? extends Fragment>, String> sFragmentToMainMenuAnchorMap =
            new ArrayMap<>();

    static {
        // Root path mappings pointing to the top-level main settings fragment.
        registerMapping("", MainSettings.class);
        registerMapping("/", MainSettings.class);

        // Multi Column Settings Categories
        // --------------------------------

        // You and Google
        //
        // TODO(crbug.com/542745585): Handle /account differently based on local state
        // since it's not a static page which is always available.
        registerMapping("/account", ManageSyncSettings.class);
        registerMapping("/account/personalize", PersonalizeGoogleServicesSettings.class);
        registerMapping("/googleServices", GoogleServicesSettings.class);
        registerMapping("/googleServices/priceTracking", PriceNotificationSettingsFragment.class);
        registerMapping("/googleServices/contextualSearch", ContextualSearchSettingsFragment.class);

        // Basics
        registerMapping("/search", SearchEngineSettings.class);
        registerMapping("/search/siteSearch", SiteSearchSettings.class);

        // Privacy & Security
        registerMapping("/privacy", PrivacySettings.class);
        registerMapping("/privacy/universalOptOut", UniversalOptOutSettings.class);
        registerMapping("/privacyGuide", PrivacyGuideFragment.class);
        registerMapping("/clearBrowsingData", ClearBrowsingDataFragment.class);
        registerMapping("/cookies", CookieSettings.class);
        registerMapping("/doNotTrack", DoNotTrackSettings.class);
        registerMapping("/preloadPages", PreloadPagesSettingsFragment.class);
        registerMapping("/preloadPages/standard", StandardPreloadingSettingsFragment.class);
        registerMapping("/preloadPages/extended", ExtendedPreloadingSettingsFragment.class);

        // Security
        registerMapping("/safeBrowsing", SafeBrowsingSettingsFragment.class);
        registerMapping("/safeBrowsing/enhanced", EnhancedProtectionSettingsFragment.class);
        registerMapping("/safeBrowsing/standard", StandardProtectionSettingsFragment.class);
        registerMapping("/httpsFirstMode", HttpsFirstModeSettingsFragment.class);
        registerMapping("/secureDns", SecureDnsSettings.class);

        // Safety Check / Safety Hub
        registerMapping("/safetyCheck", SafetyHubFragment.class);
        registerMapping("/safetyCheck/permissions", SafetyHubPermissionsFragment.class);
        registerMapping("/safetyCheck/notifications", SafetyHubNotificationsFragment.class);

        // Autofill & Passwords
        registerMapping("/autofill", AutofillAndPasswordsFragment.class);
        registerMapping("/payments", AutofillPaymentMethodsFragment.class);
        registerMapping("/payments/nonCardMethods", NonCardPaymentMethodsManagementFragment.class);
        registerMapping("/payments/financialAccounts", FinancialAccountsManagementFragment.class);
        registerMapping("/cardBenefits", AutofillCardBenefitsFragment.class);
        registerMapping("/cardBenefits/bnpl", AutofillBuyNowPayLaterFragment.class);
        registerMapping("/paymentApps", AndroidPaymentAppsFragment.class);
        registerMapping("/addresses", AutofillProfilesFragment.class);
        registerMapping("/autofill/identityDocs", AutofillIdentityDocsFragment.class);
        registerMapping("/autofill/travel", AutofillTravelFragment.class);
        registerMapping("/autofill/shopping", AutofillShoppingFragment.class);
        registerMapping("/autofill/personalContext", AutofillPersonalContextFragment.class);
        registerMapping("/autofill/settings", AutofillOptionsFragment.class)
                // The page asserts on its referrer extra.
                .withDefaults(
                        bundle -> {
                            if (!bundle.containsKey(
                                    AutofillOptionsFragment.AUTOFILL_OPTIONS_REFERRER)) {
                                bundle.putInt(
                                        AutofillOptionsFragment.AUTOFILL_OPTIONS_REFERRER,
                                        AutofillOptionsReferrer.SETTINGS);
                            }
                        });

        // Tabs and tab groups
        registerMapping("/tabs", TabsSettings.class);
        registerMapping("/inactiveTabs", TabArchiveSettingsFragment.class);

        // Homepage
        registerMapping("/homepage", HomepageSettings.class);

        // Appearance
        registerMapping("/appearance", AppearanceSettingsFragment.class);
        registerMapping("/bookmarkBar", BookmarkBarSettingsFragment.class);
        registerMapping("/theme", ThemeSettingsFragment.class);
        registerMapping("/toolbar", AdaptiveToolbarSettingsFragment.class);

        // Accessibility
        registerMapping("/accessibility", AccessibilitySettings.class);
        registerMapping("/savedZoomForSites", ImageDescriptionsSettings.class);

        // Content / Site Settings
        registerMapping("/siteSettings", SiteSettings.class);
        registerMapping("/siteSettings/category", SingleCategorySettings.class)
                .requireArgs(SingleCategorySettings.EXTRA_CATEGORY)
                .fallback("/siteSettings")
                .validateWith(
                        args -> {
                            String category =
                                    args.getString(SingleCategorySettings.EXTRA_CATEGORY, "");
                            // A Url can name any category, including one this build does not
                            // have. There is no page for it, so offer the list of categories.
                            if (!SiteSettingsCategory.isValidPreferenceKey(category)) {
                                return createUrlForFragment(SiteSettings.class, /* args= */ null);
                            }
                            // "All sites", "Storage" and "Zoom" are rendered by AllSiteSettings;
                            // SingleCategorySettings throws for them.
                            return isAllSitesCategory(category)
                                    ? createUrlForFragment(AllSiteSettings.class, args)
                                    : null;
                        });
        registerMapping("/allSites", AllSiteSettings.class)
                // The mirror image of the above: AllSiteSettings throws for a per-category key.
                // An unknown category is decoration here rather than an error, since the page
                // falls back to "All sites" for anything it does not render, so only a real
                // per-category key is sent on. Redirecting on an unknown one would hand it to a
                // page that cannot show it either, and it would be carried back here in a loop.
                .validateWith(
                        args -> {
                            String category = args.getString(AllSiteSettings.EXTRA_CATEGORY);
                            return SiteSettingsCategory.isValidPreferenceKey(category)
                                            && !isAllSitesCategory(category)
                                    ? createUrlForFragment(SingleCategorySettings.class, args)
                                    : null;
                        });
        registerMapping("/allSites/group", GroupedWebsitesSettings.class)
                .requireArgs(GroupedWebsitesSettings.EXTRA_GROUP)
                .fallback("/allSites");
        registerMapping("/siteDetails", SingleWebsiteSettings.class)
                .requireArgs(SingleWebsiteSettings.EXTRA_SITE_ADDRESS)
                .fallback("/allSites");
        registerMapping("/storageAccess", StorageAccessSubpageSettings.class)
                // The page shows one origin's storage access permissions, either the allowed or
                // the blocked ones, so it needs both to know what to show.
                .requireArgs(
                        SingleWebsiteSettings.EXTRA_SITE_ADDRESS,
                        StorageAccessSubpageSettings.EXTRA_ALLOWED)
                .fallback("/allSites");
        registerMapping("/locationPermission", LocationPermissionSubpageSettings.class)
                .requireArgs(SingleWebsiteSettings.EXTRA_SITE_ADDRESS)
                .fallback("/allSites");
        // ChosenObjectSettings is deliberately not registered. It is identified by a serialized
        // device descriptor with no stable short form, so it has no meaningful URL. It is opened
        // from SingleCategorySettings by a fragment transaction instead.

        // These pages are attached at runtime, e.g. by tapping a row in "All sites", instead of
        // being declared with android:fragment in a preference XML. They therefore have no entry
        // in the settings search index and no breadcrumb path to derive a main menu row from, so
        // the row they belong under is declared explicitly.
        registerMainMenuAnchor(SingleWebsiteSettings.class, SiteSettings.MAIN_MENU_KEY);
        registerMainMenuAnchor(GroupedWebsitesSettings.class, SiteSettings.MAIN_MENU_KEY);
        registerMainMenuAnchor(StorageAccessSubpageSettings.class, SiteSettings.MAIN_MENU_KEY);
        registerMainMenuAnchor(LocationPermissionSubpageSettings.class, SiteSettings.MAIN_MENU_KEY);
        registerMainMenuAnchor(ChosenObjectSettings.class, SiteSettings.MAIN_MENU_KEY);

        // Languages, Downloads, Tabs, Homepage
        //
        // Only the top-level languages page is URL routed. The subpages (language picker, always
        // and never translate lists) exchange the selected language using the androidx Fragment
        // Result API, which requires the calling fragment to remain on the fragment back stack.
        // URL navigation replaces detail fragments with addToBackStack=false, which breaks that
        // contract. See crbug.com/555347875; these routes are restored once the subpages no longer
        // depend on the fragment back stack.
        registerMapping("/languages", LanguageSettings.class);
        registerMapping("/downloads", DownloadSettings.class);

        // About & Developer
        registerMapping("/about", AboutChromeSettings.class);
        registerMapping("/about/legal", LegalInformationSettings.class);
        registerMapping("/developer", DeveloperSettings.class);
        registerMapping("/developer/tracing", TracingSettings.class);
        registerMapping("/developer/tracing/categories", TracingCategoriesSettings.class)
                .requireArgs(TracingCategoriesSettings.EXTRA_CATEGORY_TYPE)
                .fallback("/developer/tracing");

        // Glic
        registerMapping("/ai/gemini", GlicSettings.class);
        registerMapping("/ai/gemini/permissions", GlicActorLoginPermissionsFragment.class);

        // Parameter translations mapping URL query string keys to Fragment
        // argument extra keys with appropriate type deserialization.
        registerWebsiteAddressParameterMapping("site", SingleWebsiteSettings.EXTRA_SITE_ADDRESS);
        registerBooleanParameterMapping("fromGrouped", SingleWebsiteSettings.EXTRA_FROM_GROUPED);
        registerBooleanParameterMapping("allowed", StorageAccessSubpageSettings.EXTRA_ALLOWED);
        registerParameterMapping("category", SingleCategorySettings.EXTRA_CATEGORY);
        registerParameterMapping("title", SingleCategorySettings.EXTRA_TITLE);
        registerParameterMapping("group", GroupedWebsitesSettings.EXTRA_GROUP);
        registerIntParameterMapping(
                "referrer",
                AutofillAndPasswordsFragment.EXTRA_REFERRER,
                /* defaultValue= */ AutofillSettingsReferrer.SETTINGS_MENU);
        registerIntParameterMapping(
                "optionsReferrer",
                AutofillOptionsFragment.AUTOFILL_OPTIONS_REFERRER,
                /* defaultValue= */ AutofillOptionsReferrer.SETTINGS);
        registerIntParameterMapping(
                "categoryType",
                TracingCategoriesSettings.EXTRA_CATEGORY_TYPE,
                /* defaultValue= */ TracingSettings.CategoryType.DEFAULT);
        registerIntParameterMapping(
                "accessPoint",
                SafeBrowsingSettingsFragment.ACCESS_POINT,
                /* defaultValue= */ SettingsAccessPoint.DEFAULT);
    }

    /**
     * Registers {@code path} as the URL for {@code detailFragmentClass}.
     *
     * @return the route's spec, for declaring required arguments and a fallback.
     */
    @CanIgnoreReturnValue
    private static RouteSpec registerMapping(
            String path, Class<? extends Fragment> detailFragmentClass) {
        // Normalize lookup keys to lowercase US locale so path matching
        // remains case-insensitive and insensitive to system locale settings
        // (avoiding Turkish dotted/dotless i issues).
        String normalizedPath = path.toLowerCase(Locale.US);

        // Store canonical subpage path preserving original casing for clean
        // reverse URL generation.
        if (!normalizedPath.equals("/")) {
            String canonicalPath = path.startsWith("/") ? path.substring(1) : path;
            sFragmentToPathMap.put(detailFragmentClass, canonicalPath);
        }

        RouteSpec spec = new RouteSpec(detailFragmentClass);
        sPathToRouteSpecMap.put(normalizedPath, spec);
        return spec;
    }

    private static void registerMainMenuAnchor(
            Class<? extends Fragment> fragmentClass, String mainMenuKey) {
        sFragmentToMainMenuAnchorMap.put(fragmentClass, mainMenuKey);
    }

    public static @Nullable String getMainMenuAnchor(Class<? extends Fragment> fragmentClass) {
        return sFragmentToMainMenuAnchorMap.get(fragmentClass);
    }

    private static void registerParameterMapping(
            String queryParam, String argKey, ParameterValueParser parser) {
        sQueryParamToArgKeyMap.put(queryParam, argKey);
        sArgKeyToQueryParamMap.put(argKey, queryParam);
        sQueryParamParsers.put(queryParam, parser);
    }

    private static void registerParameterMapping(String queryParam, String argKey) {
        registerParameterMapping(
                queryParam, argKey, (bundle, key, val) -> bundle.putString(key, val));
    }

    @SuppressWarnings("unused")
    private static void registerBooleanParameterMapping(String queryParam, String argKey) {
        registerParameterMapping(
                queryParam,
                argKey,
                (bundle, key, val) -> bundle.putBoolean(key, Boolean.parseBoolean(val)));
    }

    private static void registerIntParameterMapping(
            String queryParam, String argKey, int defaultValue) {
        registerParameterMapping(
                queryParam,
                argKey,
                (bundle, key, val) -> {
                    try {
                        bundle.putInt(key, Integer.parseInt(val));
                    } catch (NumberFormatException e) {
                        bundle.putInt(key, defaultValue);
                    }
                });
    }

    // Currently unused: its only caller was the "potentialLanguages" mapping, removed along with
    // the languages subpage routes. Retained for when those routes are re-landed.
    @SuppressWarnings("unused")
    private static void registerShortParameterMapping(
            String queryParam, String argKey, short defaultValue) {
        registerParameterMapping(
                queryParam,
                argKey,
                (bundle, key, val) -> {
                    try {
                        bundle.putShort(key, Short.parseShort(val));
                    } catch (NumberFormatException e) {
                        bundle.putShort(key, defaultValue);
                    }
                });
    }

    private static void registerWebsiteAddressParameterMapping(String queryParam, String argKey) {
        registerParameterMapping(
                queryParam,
                argKey,
                (bundle, key, val) -> {
                    WebsiteAddress address = WebsiteAddress.create(val);
                    if (address != null) {
                        bundle.putSerializable(key, address);
                    }
                });
    }

    public static void registerMappingForTesting(
            String path, Class<? extends Fragment> detailFragmentClass) {
        registerMapping(path, detailFragmentClass);
    }

    /**
     * Resolves a chrome://settings URL string to a target Fragment class.
     *
     * @param url Target URL to resolve.
     * @return Resolved Fragment class token, or null if invalid.
     */
    public static @Nullable Class<? extends Fragment> getFragmentClassForUrl(String url) {
        RouteSpec spec = getRouteSpecForUrl(url);
        return spec == null ? null : spec.mFragmentClass;
    }

    /**
     * Returns the normalized registry path of a settings URL, or null if it is not one.
     *
     * <p>The scheme is validated to prevent cross-origin or local file scheme injection, and the
     * host to ensure only settings pages are routed through this registry. The path is lowercased
     * with {@link Locale#US} so matching is insensitive to the device locale, and a trailing slash
     * is stripped so "chrome://settings/appearance/" matches "chrome://settings/appearance".
     */
    private static @Nullable String settingsPath(Uri uri) {
        if (!UrlUtilities.isChromeScheme(uri.getScheme())) return null;
        if (!UrlConstants.SETTINGS_HOST.equalsIgnoreCase(uri.getHost())) return null;

        String path = uri.getPath();
        if (path == null) return "";

        path = path.toLowerCase(Locale.US);
        return path.endsWith("/") ? path.substring(0, path.length() - 1) : path;
    }

    /** {@link #settingsPath} for a URL string. */
    private static @Nullable String settingsPathForUrl(@Nullable String url) {
        return url == null ? null : settingsPath(Uri.parse(url));
    }

    /**
     * Returns whether two URLs address the same settings page, i.e. the same path and query.
     *
     * <p>Comparison ignores the scheme, because chrome://settings and chrome-native://settings both
     * reach settings and are used interchangeably today; see the TODO in {@link
     * #createUrlForFragment}. Returns false unless both URLs are settings URLs.
     */
    public static boolean isSameSettingsPage(@Nullable String url, @Nullable String other) {
        String key = settingsPageKey(url);
        return key != null && key.equals(settingsPageKey(other));
    }

    /** Returns a scheme-independent identity for a settings URL, or null if it is not one. */
    private static @Nullable String settingsPageKey(@Nullable String url) {
        if (url == null) return null;

        Uri uri = Uri.parse(url);
        String path = settingsPath(uri);
        if (path == null) return null;

        String query = uri.getQuery();
        return query == null ? path : path + "?" + query;
    }

    /**
     * Extracts URL query parameters into a Fragment argument Bundle.
     *
     * @param url URL string containing optional query parameters.
     * @return Populated {@link Bundle} containing typed parameter extras.
     */
    public static Bundle parseUrlArguments(String url) {
        Bundle bundle = new Bundle();
        if (url.isEmpty()) return bundle;

        try {
            Uri uri = Uri.parse(url);
            if (!uri.isHierarchical()) return bundle;

            Set<String> queryNames = uri.getQueryParameterNames();
            for (String param : queryNames) {
                String val = uri.getQueryParameter(param);
                // "?site=" and "?site" both parse to an empty value, which names nothing. Treating
                // it as absent is what every reader of these arguments already expects, and it
                // keeps the required argument check in resolve() a question of presence alone.
                if (val == null || val.isEmpty()) continue;

                // Map query parameter key to argument bundle key if registered,
                // otherwise keep original.
                String argKey = sQueryParamToArgKeyMap.getOrDefault(param, param);
                ParameterValueParser parser = sQueryParamParsers.get(param);
                if (parser != null) {
                    parser.putValue(bundle, argKey, val);
                } else {
                    bundle.putString(argKey, val);
                }
            }
        } catch (UnsupportedOperationException
                | IllegalArgumentException
                | IndexOutOfBoundsException e) {
            // Malformed URIs safely fail with an empty bundle to prevent partial state.
            return new Bundle();
        }

        // Populate default mandatory argument extras for specific target
        // fragments if omitted in the URL query parameters. Some fragments
        // (such as ThemeSettingsFragment) enforce assertions on mandatory
        // entry point extras during fragment creation.
        applyDefaultArguments(url, bundle);
        return bundle;
    }

    /** Applies the route's default arguments for anything the URL left out. */
    private static void applyDefaultArguments(String url, Bundle bundle) {
        RouteSpec spec = getRouteSpecForUrl(url);
        if (spec != null && spec.mDefaultsProvider != null) {
            spec.mDefaultsProvider.accept(bundle);
        }
    }

    /** Returns the route spec for a settings URL, or null if the URL is not routed. */
    private static @Nullable RouteSpec getRouteSpecForUrl(String url) {
        String path = settingsPathForUrl(url);
        return path == null ? null : sPathToRouteSpecMap.get(path);
    }

    /**
     * Resolves a settings URL to the page to show, or to a URL to go to instead.
     *
     * <p>A redirect is returned when the URL cannot produce a usable page: a required argument is
     * absent, or an argument names something the page cannot render. This is the norm rather than
     * the exception for settings URLs, which are user editable and are replayed from history after
     * the data they point at may have been deleted. Callers must honour the redirect instead of
     * instantiating a page, since the pages themselves respond to missing arguments by crashing.
     */
    public static Resolution resolve(String url) {
        Bundle args = parseUrlArguments(url);
        RouteSpec spec = getRouteSpecForUrl(url);

        if (spec == null) {
            // An unknown path is more often a mistyped or truncated version of a real one than a
            // page in its own right, so offer the closest thing the user asked for before giving
            // up: chrome://settings/siteSettings/nonsense belongs on Site settings.
            String ancestorUrl = nearestRegisteredAncestorUrl(url);
            if (ancestorUrl != null) {
                return new Resolution(/* fragmentClass= */ null, args, ancestorUrl);
            }

            // Nothing recognisable is left, so show the main settings page, matching what the
            // omnibox does for any other unrecognised chrome:// path.
            return new Resolution(MainSettings.class, args, /* redirectUrl= */ null);
        }

        for (String requiredKey : spec.mRequiredArgKeys) {
            if (!args.containsKey(requiredKey)) {
                return new Resolution(
                        /* fragmentClass= */ null, args, settingsUrlForPath(spec.mFallbackPath));
            }
        }

        if (spec.mValidator != null) {
            String redirectUrl = spec.mValidator.validate(args);
            if (redirectUrl != null) {
                return new Resolution(/* fragmentClass= */ null, args, redirectUrl);
            }
        }

        return new Resolution(spec.mFragmentClass, args, /* redirectUrl= */ null);
    }

    /** Returns the settings URL for a registered path, defaulting to the settings root. */
    private static String settingsUrlForPath(@Nullable String path) {
        String root = UrlConstants.CHROME_URL_PREFIX + UrlConstants.SETTINGS_HOST;
        return path == null ? root : root + path;
    }

    /**
     * Returns the Url of the closest ancestor path that is a page, or null if there is none.
     *
     * <p>Paths are walked one segment at a time, so "/siteSettings/category&title=Location" offers
     * "/siteSettings". The canonical Url is rebuilt from the fragment so the redirect keeps the
     * registered casing rather than the lowercased form used for matching.
     */
    private static @Nullable String nearestRegisteredAncestorUrl(String url) {
        String path = settingsPathForUrl(url);
        if (path == null) return null;

        // Stop before index 0: an empty ancestor is the settings root, which the caller handles.
        for (int slash = path.lastIndexOf('/');
                slash > 0;
                slash = path.lastIndexOf('/', slash - 1)) {
            String ancestor = path.substring(0, slash);
            RouteSpec spec = sPathToRouteSpecMap.get(ancestor);
            if (spec == null) continue;

            String canonicalUrl = createUrlForFragment(spec.mFragmentClass, /* args= */ null);
            return canonicalUrl != null ? canonicalUrl : settingsUrlForPath(ancestor);
        }
        return null;
    }

    /**
     * Returns whether {@code categoryPreferenceKey} names one of the categories rendered by {@link
     * AllSiteSettings} rather than {@link SingleCategorySettings}. Each page throws when given the
     * other's categories.
     */
    private static boolean isAllSitesCategory(@Nullable String categoryPreferenceKey) {
        if (categoryPreferenceKey == null) return false;
        return categoryPreferenceKey.equals(
                        SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.ALL_SITES))
                || categoryPreferenceKey.equals(
                        SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.USE_STORAGE))
                || categoryPreferenceKey.equals(
                        SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.ZOOM));
    }

    /**
     * Creates a canonical URL for a Fragment class and argument Bundle.
     *
     * @param fragmentClass Target fragment class to represent.
     * @param args Optional argument bundle containing query parameters.
     * @return Canonical chrome://settings URL string, or null if unmapped.
     */
    public static @Nullable String createUrlForFragment(
            Class<? extends Fragment> fragmentClass, @Nullable Bundle args) {
        String path = getUrlPathForFragmentClass(fragmentClass);
        if (path == null && !MainSettings.class.equals(fragmentClass)) {
            return null;
        }

        // TODO(mwoj): This will resolve to chrome://settings, but we should consistently pick
        // chrome-native://settings or chrome://settings so the Urls don't mix and match when
        // clicking through preference fragments or manually entering Urls.
        String chromeSettingsUrl = UrlConstants.CHROME_URL_PREFIX + UrlConstants.SETTINGS_HOST;
        String targetUrl = chromeSettingsUrl + "/" + (path != null ? path : "");

        // If there are no supplied arguments, just return the page.
        // (e.g., chrome://settings/about).
        if (args == null || args.isEmpty()) {
            return targetUrl;
        }

        // Otherwise, bake the arguments into url.
        // (e.g., chrome://settings/siteDetails?site=foo.com)
        Uri.Builder builder = Uri.parse(targetUrl).buildUpon();

        // Sort the keys here for unit tests, so that generated strings
        // will be consistent.
        List<String> sortedKeys = new ArrayList<>(args.keySet());
        Collections.sort(sortedKeys);
        for (String key : sortedKeys) {
            Object val = args.get(key);
            if (val == null) continue;

            UrlParam result = extractQueryParam(key, val);
            if (result != null) {
                String queryParam = sArgKeyToQueryParamMap.getOrDefault(result.mKey, result.mKey);
                builder.appendQueryParameter(queryParam, result.mValue);
            }
        }
        return builder.build().toString();
    }

    /**
     * Returns whether {@code url} carries every argument in {@code args} back with the same key,
     * type and value.
     *
     * <p>Under URL navigation the URL is the source of truth: the page is rebuilt from it when the
     * tab is restored, when the user walks back to it, and whenever it is replayed from history. An
     * argument the URL cannot carry is therefore not merely absent from the first navigation, it is
     * absent from every later one. A caller holding both a URL and the {@link Bundle} it was built
     * from must ask this before treating the URL as a replacement for the Bundle.
     *
     * <p>{@link #createUrlForFragment} is deliberately permissive: an argument whose key has no
     * registered query parameter is still written out, under its raw key, as long as its value is a
     * CharSequence, Number or Boolean. {@link #parseUrlArguments} is permissive in the same way,
     * but it has no type information to restore with, so it puts the value back as a String. An int
     * or boolean argument therefore survives the round trip in name only, and the page reads 0 or
     * false from it. Registering the argument with {@link #registerIntParameterMapping} and its
     * siblings is what makes the round trip typed; this method is how a caller - and the next
     * engineer to add a page - finds out that it has not been done.
     */
    public static boolean urlPreservesArgs(String url, @Nullable Bundle args) {
        if (args == null || args.isEmpty()) return true;

        Bundle parsed = parseUrlArguments(url);
        for (String key : args.keySet()) {
            Object original = args.get(key);
            // A null value carries no information, so nothing is lost by not carrying it.
            if (original == null) continue;

            Object restored = parsed.get(key);
            if (restored == null) return false;

            // Compare types as well as values: an int written as "3" comes back as the String "3"
            // unless the argument has a registered typed parser, and Bundle#getInt would then
            // silently return 0 rather than 3.
            if (original.getClass() != restored.getClass()) return false;

            if (!original.equals(restored)) return false;
        }
        return true;
    }

    private static @Nullable UrlParam extractQueryParam(String key, Object val) {
        if (val instanceof Website website) {
            return new UrlParam(
                    SingleWebsiteSettings.EXTRA_SITE_ADDRESS, website.getAddress().getOrigin());
        }
        if (val instanceof WebsiteAddress websiteAddress) {
            return new UrlParam(
                    SingleWebsiteSettings.EXTRA_SITE_ADDRESS, websiteAddress.getOrigin());
        }
        if (val instanceof WebsiteGroup websiteGroup) {
            return new UrlParam(
                    GroupedWebsitesSettings.EXTRA_GROUP, websiteGroup.getDomainAndRegistry());
        }
        if (val instanceof CharSequence || val instanceof Number || val instanceof Boolean) {
            return new UrlParam(key, String.valueOf(val));
        }
        return null;
    }

    private static class UrlParam {
        final String mKey;
        final String mValue;

        UrlParam(String key, String value) {
            this.mKey = key;
            this.mValue = value;
        }
    }

    /**
     * Returns the canonical URL path for a given Fragment class.
     *
     * @param fragmentClass Fragment class token to look up.
     * @return Canonical path string without leading slash, or null if unmapped.
     */
    public static @Nullable String getUrlPathForFragmentClass(
            Class<? extends Fragment> fragmentClass) {
        return sFragmentToPathMap.get(fragmentClass);
    }
}
