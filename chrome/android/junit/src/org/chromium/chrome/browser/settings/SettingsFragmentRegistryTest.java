// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import androidx.fragment.app.Fragment;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.about_settings.LegalInformationSettings;
import org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment;
import org.chromium.chrome.browser.appearance.settings.BookmarkBarSettingsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillAndPasswordsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillAndPasswordsFragment.AutofillSettingsReferrer;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsReferrer;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.commerce.PriceNotificationSettingsFragment;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSettingsFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicActorLoginPermissionsFragment;
import org.chromium.chrome.browser.night_mode.NightModeMetrics;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.ExtendedPreloadingSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.StandardPreloadingSettingsFragment;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.privacy.settings.UniversalOptOutSettings;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.EnhancedProtectionSettingsFragment;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safe_browsing.settings.StandardProtectionSettingsFragment;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.browser.tracing.settings.TracingCategoriesSettings;
import org.chromium.chrome.browser.tracing.settings.TracingSettings;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.ChosenObjectSettings;
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

import java.util.Collections;
import java.util.Map;

/** Unit tests for {@link SettingsFragmentRegistry}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp")
@EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
public class SettingsFragmentRegistryTest {

    @Test
    public void testRegistryInitialization() throws Exception {
        // Verify root path
        assertEquals(MainSettings.class, fragmentClassForPath(""));
        assertEquals(MainSettings.class, fragmentClassForPath("/"));
        assertNull(fragmentClassForPath("/results"));

        // Verify some notable mappings
        assertEquals(PrivacySettings.class, fragmentClassForPath("/privacy"));
        assertEquals(AppearanceSettingsFragment.class, fragmentClassForPath("/appearance"));
        assertEquals(BookmarkBarSettingsFragment.class, fragmentClassForPath("/bookmarkBar"));
        assertEquals(ThemeSettingsFragment.class, fragmentClassForPath("/theme"));
        assertEquals(SafeBrowsingSettingsFragment.class, fragmentClassForPath("/safebrowsing"));

        // Verify canonical path mapping
        Map<Class<? extends Fragment>, String> fragmentMap =
                SettingsFragmentRegistry.sFragmentToPathMap;
        assertEquals("privacy", fragmentMap.get(PrivacySettings.class));
        assertEquals("appearance", fragmentMap.get(AppearanceSettingsFragment.class));
        assertEquals("bookmarkBar", fragmentMap.get(BookmarkBarSettingsFragment.class));
        assertEquals("theme", fragmentMap.get(ThemeSettingsFragment.class));
        assertEquals("privacy/universalOptOut", fragmentMap.get(UniversalOptOutSettings.class));
        assertEquals(
                "googleServices/priceTracking",
                fragmentMap.get(PriceNotificationSettingsFragment.class));
        assertEquals(
                "googleServices/contextualSearch",
                fragmentMap.get(ContextualSearchSettingsFragment.class));
        assertEquals(
                "ai/gemini/permissions", fragmentMap.get(GlicActorLoginPermissionsFragment.class));
        assertEquals(
                "developer/tracing/categories", fragmentMap.get(TracingCategoriesSettings.class));

        // Verify TracingCategoriesSettings required args and fallback
        SettingsFragmentRegistry.Resolution missingTypeRes =
                SettingsFragmentRegistry.resolve("chrome://settings/developer/tracing/categories");
        assertEquals("chrome://settings/developer/tracing", missingTypeRes.redirectUrl);

        SettingsFragmentRegistry.Resolution validTypeRes =
                SettingsFragmentRegistry.resolve(
                        "chrome://settings/developer/tracing/categories?categoryType="
                                + TracingSettings.CategoryType.NON_DEFAULT);
        assertNull(validTypeRes.redirectUrl);
        assertEquals(TracingCategoriesSettings.class, validTypeRes.fragmentClass);
        assertEquals(
                TracingSettings.CategoryType.NON_DEFAULT,
                validTypeRes.args.getInt(TracingCategoriesSettings.EXTRA_CATEGORY_TYPE));
    }

    @Test
    public void testRegisterMappingForTesting() throws Exception {
        Map<Class<? extends Fragment>, String> fragmentMap =
                SettingsFragmentRegistry.sFragmentToPathMap;

        String testPath = "/test/custom";
        Class<? extends Fragment> testClass = Fragment.class;

        // Ensure it doesn't exist yet
        assertNull(fragmentClassForPath(testPath));

        // Register the new mapping
        SettingsFragmentRegistry.registerMappingForTesting(testPath, testClass);

        // Verify it was added
        assertEquals(testClass, fragmentClassForPath(testPath));
        assertEquals("test/custom", fragmentMap.get(testClass));

        // Clean up
        SettingsFragmentRegistry.sPathToRouteSpecMap.remove(testPath);
        fragmentMap.remove(testClass);
    }

    @Test
    public void testParameterMappings() throws Exception {
        Map<String, String> queryMap = SettingsFragmentRegistry.sQueryParamToArgKeyMap;
        Map<String, String> argMap = SettingsFragmentRegistry.sArgKeyToQueryParamMap;

        assertEquals(SingleWebsiteSettings.EXTRA_SITE_ADDRESS, queryMap.get("site"));
        assertEquals("site", argMap.get(SingleWebsiteSettings.EXTRA_SITE_ADDRESS));

        assertEquals(SingleWebsiteSettings.EXTRA_FROM_GROUPED, queryMap.get("fromGrouped"));
        assertEquals("fromGrouped", argMap.get(SingleWebsiteSettings.EXTRA_FROM_GROUPED));

        assertEquals(GroupedWebsitesSettings.EXTRA_GROUP, queryMap.get("group"));
        assertEquals("group", argMap.get(GroupedWebsitesSettings.EXTRA_GROUP));
    }

    @Test
    public void testRegisterMappingCaseInsensitive() throws Exception {
        // Registering with mixed case should be lowercased in the registry map
        SettingsFragmentRegistry.registerMappingForTesting("/Test/Case", Fragment.class);

        Map<Class<? extends Fragment>, String> fragmentMap =
                SettingsFragmentRegistry.sFragmentToPathMap;

        // The Url is matched case insensitively, however it was registered or is written.
        assertEquals(Fragment.class, fragmentClassForPath("/test/case"));
        assertEquals(Fragment.class, fragmentClassForPath("/Test/Case"));

        // Canonical path map should preserve casing of the subpage path (without the slash)
        assertEquals("Test/Case", fragmentMap.get(Fragment.class));

        // Clean up
        SettingsFragmentRegistry.sPathToRouteSpecMap.remove("/test/case");
        fragmentMap.remove(Fragment.class);
    }

    @Test
    public void testValidUrls() {
        assertEquals(
                AppearanceSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/appearance"));
        assertEquals(
                BookmarkBarSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/bookmarkBar"));
        assertEquals(
                ThemeSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/theme"));
        assertEquals(
                MainSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings"));
        assertEquals(
                MainSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/"));
    }

    @Test
    public void testCaseInsensitivePathLookup() {
        assertEquals(
                AppearanceSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/APPEARANCE"));
        assertEquals(
                LegalInformationSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/ABOUT/LEGAL"));
    }

    @Test
    public void testInvalidSchemesAndHostsRejected() {
        assertNull(
                SettingsFragmentRegistry.getFragmentClassForUrl("https://google.com/appearance"));
        assertNull(SettingsFragmentRegistry.getFragmentClassForUrl("file:///appearance"));
        assertNull(SettingsFragmentRegistry.getFragmentClassForUrl("chrome://history/appearance"));
    }

    @Test
    public void testTrailingSlashNormalization() {
        assertEquals(
                AppearanceSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/appearance/"));
        assertEquals(
                LegalInformationSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/about/legal/"));
    }

    @Test
    public void testParseUrlArguments() {
        assertTrue(SettingsFragmentRegistry.parseUrlArguments("").isEmpty());

        Bundle bundle =
                SettingsFragmentRegistry.parseUrlArguments(
                        "chrome://settings/siteDetails?site=example.com&fromGrouped=true");
        assertEquals(
                WebsiteAddress.create("example.com"),
                bundle.getSerializable(SingleWebsiteSettings.EXTRA_SITE_ADDRESS));
        assertTrue(bundle.getBoolean(SingleWebsiteSettings.EXTRA_FROM_GROUPED));

        Bundle groupBundle =
                SettingsFragmentRegistry.parseUrlArguments(
                        "chrome://settings/allSites/group?group=example.com");
        assertEquals("example.com", groupBundle.getString(GroupedWebsitesSettings.EXTRA_GROUP));

        Bundle autofillBundle =
                SettingsFragmentRegistry.parseUrlArguments(
                        "chrome://settings/autofill?referrer="
                                + AutofillSettingsReferrer.SETTINGS_SEARCH);
        assertEquals(
                AutofillSettingsReferrer.SETTINGS_SEARCH,
                autofillBundle.getInt(AutofillAndPasswordsFragment.EXTRA_REFERRER));

        // Non-hierarchical URIs return an empty bundle safely.
        assertTrue(SettingsFragmentRegistry.parseUrlArguments("mailto:user@example.com").isEmpty());
    }

    @Test
    public void testTypedQueryParameterParsing() {
        Bundle bundle =
                SettingsFragmentRegistry.parseUrlArguments(
                        "chrome://settings/siteDetails?site=example.com&fromGrouped=true");
        assertTrue(bundle.getBoolean(SingleWebsiteSettings.EXTRA_FROM_GROUPED));

        // Malformed numeric parameters should fall back to known good default values.
        Bundle malformedBundle =
                SettingsFragmentRegistry.parseUrlArguments(
                        "chrome://settings/autofill?referrer=abc&optionsReferrer=xyz");
        assertEquals(
                AutofillSettingsReferrer.SETTINGS_MENU,
                malformedBundle.getInt(AutofillAndPasswordsFragment.EXTRA_REFERRER));
        assertEquals(
                AutofillOptionsReferrer.SETTINGS,
                malformedBundle.getInt(AutofillOptionsFragment.AUTOFILL_OPTIONS_REFERRER));

        Bundle unknownBundle =
                SettingsFragmentRegistry.parseUrlArguments(
                        "chrome://settings/foo?boolKey=true&intKey=42");
        assertEquals("true", unknownBundle.getString("boolKey"));
        assertEquals("42", unknownBundle.getString("intKey"));
    }

    @Test
    public void testCreateUrlForFragment() {
        Bundle args = new Bundle();
        args.putString(SingleWebsiteSettings.EXTRA_SITE_ADDRESS, "example.com");
        String url =
                SettingsFragmentRegistry.createUrlForFragment(SingleWebsiteSettings.class, args);
        assertEquals("chrome://settings/siteDetails?site=example.com", url);

        // Verify WebsiteAddress argument handling.
        Bundle addressArgs = new Bundle();
        addressArgs.putSerializable(
                SingleWebsiteSettings.EXTRA_SITE_ADDRESS,
                WebsiteAddress.create("https://example.com"));
        assertEquals(
                "chrome://settings/siteDetails?site=https%3A%2F%2Fexample.com",
                SettingsFragmentRegistry.createUrlForFragment(
                        SingleWebsiteSettings.class, addressArgs));

        // Verify Website argument handling.
        Bundle websiteArgs = new Bundle();
        websiteArgs.putSerializable(
                SingleWebsiteSettings.EXTRA_SITE_ADDRESS,
                new Website(WebsiteAddress.create("https://example.com"), null));
        assertEquals(
                "chrome://settings/siteDetails?site=https%3A%2F%2Fexample.com",
                SettingsFragmentRegistry.createUrlForFragment(
                        SingleWebsiteSettings.class, websiteArgs));

        // Verify WebsiteGroup argument handling.
        Bundle groupArgs = new Bundle();
        groupArgs.putSerializable(
                GroupedWebsitesSettings.EXTRA_GROUP,
                new WebsiteGroup("google.com", Collections.emptyList()));
        assertEquals(
                "chrome://settings/allSites/group?group=google.com",
                SettingsFragmentRegistry.createUrlForFragment(
                        GroupedWebsitesSettings.class, groupArgs));

        Bundle stringGroupArgs = new Bundle();
        stringGroupArgs.putString(GroupedWebsitesSettings.EXTRA_GROUP, "example.com");
        String groupUrl =
                SettingsFragmentRegistry.createUrlForFragment(
                        GroupedWebsitesSettings.class, stringGroupArgs);
        assertEquals("chrome://settings/allSites/group?group=example.com", groupUrl);
    }

    @Test
    public void testUrlPreservesArgs_noArgs() {
        assertTrue(SettingsFragmentRegistry.urlPreservesArgs("chrome://settings/privacy", null));
        assertTrue(
                SettingsFragmentRegistry.urlPreservesArgs(
                        "chrome://settings/privacy", new Bundle()));
    }

    @Test
    public void testUrlPreservesArgs_registeredArgsRoundTrip() {
        // A String argument under its raw key is written and read back as a String.
        Bundle stringArgs = new Bundle();
        stringArgs.putString(SingleCategorySettings.EXTRA_CATEGORY, "cookies");
        String categoryUrl =
                SettingsFragmentRegistry.createUrlForFragment(
                        SingleCategorySettings.class, stringArgs);
        assertTrue(SettingsFragmentRegistry.urlPreservesArgs(categoryUrl, stringArgs));

        // An int argument with a registered typed parser comes back as an int.
        Bundle intArgs = new Bundle();
        intArgs.putInt(
                AutofillAndPasswordsFragment.EXTRA_REFERRER,
                AutofillSettingsReferrer.SETTINGS_SEARCH);
        String referrerUrl =
                SettingsFragmentRegistry.createUrlForFragment(
                        AutofillAndPasswordsFragment.class, intArgs);
        assertTrue(SettingsFragmentRegistry.urlPreservesArgs(referrerUrl, intArgs));

        // A WebsiteAddress is rebuilt from its origin by the registered parser.
        Bundle addressArgs = new Bundle();
        addressArgs.putSerializable(
                SingleWebsiteSettings.EXTRA_SITE_ADDRESS,
                WebsiteAddress.create("https://example.com"));
        String siteUrl =
                SettingsFragmentRegistry.createUrlForFragment(
                        SingleWebsiteSettings.class, addressArgs);
        assertTrue(SettingsFragmentRegistry.urlPreservesArgs(siteUrl, addressArgs));
    }

    @Test
    public void testUrlPreservesArgs_safeBrowsingAccessPointRoundTrips() {
        // SafeBrowsingSettingsFragment's access point is an int, so it only survives the round
        // trip because "accessPoint" is registered with a typed parser.
        Bundle args =
                SafeBrowsingSettingsFragment.createArguments(SettingsAccessPoint.SAFETY_CHECK);
        String url =
                SettingsFragmentRegistry.createUrlForFragment(
                        SafeBrowsingSettingsFragment.class, args);
        assertEquals(
                "chrome://settings/safeBrowsing?accessPoint=" + SettingsAccessPoint.SAFETY_CHECK,
                url);
        assertTrue(SettingsFragmentRegistry.urlPreservesArgs(url, args));
        assertEquals(
                SettingsAccessPoint.SAFETY_CHECK,
                SettingsFragmentRegistry.parseUrlArguments(url)
                        .getInt(SafeBrowsingSettingsFragment.ACCESS_POINT));
    }

    @Test
    public void testUrlPreservesArgs_unregisteredIntLosesItsType() {
        // An int argument whose key has no registered query parameter is still written out, under
        // its raw key, but it is read back as the String "3" because there is no type information
        // to restore it with. The page would call Bundle#getInt and silently get 0, so the URL is
        // not a substitute for the Bundle here.
        Bundle args = new Bundle();
        args.putInt("UnregisteredFragment.SomeInt", 3);
        String url = SettingsFragmentRegistry.createUrlForFragment(PrivacySettings.class, args);
        assertEquals("chrome://settings/privacy?UnregisteredFragment.SomeInt=3", url);
        assertFalse(SettingsFragmentRegistry.urlPreservesArgs(url, args));
    }

    @Test
    public void testUrlPreservesArgs_uncarriableArgIsDropped() {
        // A value that is neither a CharSequence, Number, Boolean nor one of the site settings
        // types the registry knows how to spell is not written to the URL at all.
        Bundle args = new Bundle();
        args.putParcelable("uncarriable", new Bundle());
        String url = SettingsFragmentRegistry.createUrlForFragment(PrivacySettings.class, args);
        assertEquals("chrome://settings/privacy", url);
        assertFalse(SettingsFragmentRegistry.urlPreservesArgs(url, args));
    }

    @Test
    public void testParseUrlArgumentsPopulatesDefaultMandatoryExtras() {
        // Verify that parsing a theme settings URL without explicit query
        // parameters automatically populates the mandatory
        // theme_settings_entry extra expected by ThemeSettingsFragment.
        Bundle bundle = SettingsFragmentRegistry.parseUrlArguments("chrome://settings/theme");
        assertTrue(bundle.containsKey(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY));
        assertEquals(
                NightModeMetrics.ThemeSettingsEntry.SETTINGS,
                bundle.getInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY));

        // Verify that parsing an autofill options URL without explicit query
        // parameters automatically populates the mandatory
        // autofill-options-referrer extra expected by AutofillOptionsFragment.
        Bundle autofillOptionsBundle =
                SettingsFragmentRegistry.parseUrlArguments("chrome://settings/autofill/settings");
        assertTrue(
                autofillOptionsBundle.containsKey(
                        AutofillOptionsFragment.AUTOFILL_OPTIONS_REFERRER));
        assertEquals(
                AutofillOptionsReferrer.SETTINGS,
                autofillOptionsBundle.getInt(AutofillOptionsFragment.AUTOFILL_OPTIONS_REFERRER));
    }

    @Test
    public void testPreferenceNavigationUrlGeneration() {
        // Verify that root category and sublevel preference targets construct
        // canonical chrome://settings/ URLs.
        assertEquals(
                "chrome://settings/appearance",
                SettingsFragmentRegistry.createUrlForFragment(
                        AppearanceSettingsFragment.class, null));

        assertEquals(
                "chrome://settings/bookmarkBar",
                SettingsFragmentRegistry.createUrlForFragment(
                        BookmarkBarSettingsFragment.class, null));

        assertEquals(
                "chrome://settings/theme",
                SettingsFragmentRegistry.createUrlForFragment(ThemeSettingsFragment.class, null));

        assertEquals(
                "chrome://settings/privacy",
                SettingsFragmentRegistry.createUrlForFragment(PrivacySettings.class, null));

        assertEquals(
                "chrome://settings/clearBrowsingData",
                SettingsFragmentRegistry.createUrlForFragment(
                        ClearBrowsingDataFragment.class, null));

        assertEquals(
                "chrome://settings/safeBrowsing",
                SettingsFragmentRegistry.createUrlForFragment(
                        SafeBrowsingSettingsFragment.class, null));

        Bundle siteArgs = new Bundle();
        siteArgs.putString(SingleWebsiteSettings.EXTRA_SITE_ADDRESS, "https://example.com");
        assertEquals(
                "chrome://settings/siteDetails?site=https%3A%2F%2Fexample.com",
                SettingsFragmentRegistry.createUrlForFragment(
                        SingleWebsiteSettings.class, siteArgs));
    }

    @Test
    public void testHierarchicalUrlToFragmentResolution() {
        // Verify that deep link URLs properly resolve to their corresponding
        // root category and sublevel fragment classes.
        assertEquals(
                PreloadPagesSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/preloadPages"));

        assertEquals(
                StandardPreloadingSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl(
                        "chrome://settings/preloadPages/standard"));

        assertEquals(
                ExtendedPreloadingSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl(
                        "chrome://settings/preloadPages/extended"));

        assertEquals(
                SafeBrowsingSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/safeBrowsing"));

        assertEquals(
                StandardProtectionSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl(
                        "chrome://settings/safeBrowsing/standard"));

        assertEquals(
                EnhancedProtectionSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl(
                        "chrome://settings/safeBrowsing/enhanced"));

        assertEquals(
                AboutChromeSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/about"));

        assertEquals(
                LegalInformationSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/about/legal"));

        assertEquals(
                DeveloperSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/developer"));

        assertEquals(
                TracingSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl(
                        "chrome://settings/developer/tracing"));
    }

    @Test
    public void testMainMenuAnchors() {
        // Site settings subpages are attached at runtime and are absent from the search index, so
        // they declare the row they live under explicitly.
        assertEquals(
                SiteSettings.MAIN_MENU_KEY,
                SettingsFragmentRegistry.getMainMenuAnchor(SingleWebsiteSettings.class));
        assertEquals(
                SiteSettings.MAIN_MENU_KEY,
                SettingsFragmentRegistry.getMainMenuAnchor(GroupedWebsitesSettings.class));
        assertEquals(
                SiteSettings.MAIN_MENU_KEY,
                SettingsFragmentRegistry.getMainMenuAnchor(StorageAccessSubpageSettings.class));
        assertEquals(
                SiteSettings.MAIN_MENU_KEY,
                SettingsFragmentRegistry.getMainMenuAnchor(
                        LocationPermissionSubpageSettings.class));
        assertEquals(
                SiteSettings.MAIN_MENU_KEY,
                SettingsFragmentRegistry.getMainMenuAnchor(ChosenObjectSettings.class));

        // Pages reachable from a preference XML resolve their row from the breadcrumb path and do
        // not need an anchor.
        assertNull(SettingsFragmentRegistry.getMainMenuAnchor(SiteSettings.class));
        assertNull(SettingsFragmentRegistry.getMainMenuAnchor(PrivacySettings.class));
    }

    @Test
    public void testIsSameSettingsPage() {
        // Scheme is ignored: both schemes reach settings and are currently used interchangeably.
        assertTrue(
                SettingsFragmentRegistry.isSameSettingsPage(
                        "chrome://settings/allSites", "chrome-native://settings/allSites"));

        // Path comparison is case insensitive and tolerates a trailing slash, matching
        // getFragmentClassForUrl().
        assertTrue(
                SettingsFragmentRegistry.isSameSettingsPage(
                        "chrome://settings/allSites/", "chrome://settings/allsites"));

        // Query parameters are part of the page identity.
        assertTrue(
                SettingsFragmentRegistry.isSameSettingsPage(
                        "chrome://settings/allSites/group?group=example.com",
                        "chrome://settings/allSites/group?group=example.com"));
        assertFalse(
                SettingsFragmentRegistry.isSameSettingsPage(
                        "chrome://settings/allSites/group?group=example.com",
                        "chrome://settings/allSites/group?group=other.com"));
        assertFalse(
                SettingsFragmentRegistry.isSameSettingsPage(
                        "chrome://settings/allSites", "chrome://settings/siteSettings"));

        // Non-settings and malformed URLs never match, including against each other.
        assertFalse(
                SettingsFragmentRegistry.isSameSettingsPage(
                        "https://example.com/allSites", "chrome://settings/allSites"));
        assertFalse(
                SettingsFragmentRegistry.isSameSettingsPage(
                        "https://example.com/", "https://example.com/"));
        assertFalse(
                SettingsFragmentRegistry.isSameSettingsPage(null, "chrome://settings/allSites"));
        assertFalse(
                SettingsFragmentRegistry.isSameSettingsPage("chrome://settings/allSites", null));
    }

    @Test
    public void testResolveRedirectsWhenRequiredArgumentIsMissing() {
        // Each of these pages is identified entirely by an argument, so without it there is no
        // page to show. They fall back to the closest page that lists what they were showing.
        assertRedirects("chrome://settings/siteDetails", "chrome://settings/allSites");
        assertRedirects("chrome://settings/allSites/group", "chrome://settings/allSites");
        assertRedirects("chrome://settings/locationPermission", "chrome://settings/allSites");
        assertRedirects("chrome://settings/storageAccess", "chrome://settings/allSites");
        assertRedirects(
                "chrome://settings/siteSettings/category", "chrome://settings/siteSettings");
    }

    @Test
    public void testResolveShowsPageWhenRequiredArgumentIsPresent() {
        SettingsFragmentRegistry.Resolution resolution =
                SettingsFragmentRegistry.resolve(
                        "chrome://settings/siteDetails?site=https://example.com");
        assertNull(resolution.redirectUrl);
        assertEquals(SingleWebsiteSettings.class, resolution.fragmentClass);
        assertTrue(resolution.args.containsKey(SingleWebsiteSettings.EXTRA_SITE_ADDRESS));

        resolution =
                SettingsFragmentRegistry.resolve(
                        "chrome://settings/allSites/group?group=example.com");
        assertNull(resolution.redirectUrl);
        assertEquals(GroupedWebsitesSettings.class, resolution.fragmentClass);
    }

    @Test
    public void testResolveRedirectsCategoriesToThePageThatRendersThem() {
        // "All sites", "Storage" and "Zoom" are rendered by AllSiteSettings, every other category
        // by SingleCategorySettings. Each page throws when handed the other's categories, so a URL
        // naming the wrong one is sent to its counterpart rather than being shown.
        assertRedirects(
                "chrome://settings/siteSettings/category?category="
                        + SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.ALL_SITES),
                "chrome://settings/allSites?category=all_sites");
        assertRedirects(
                "chrome://settings/allSites?category="
                        + SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.CAMERA),
                "chrome://settings/siteSettings/category?category=camera");

        // The categories each page does render are shown, not redirected.
        SettingsFragmentRegistry.Resolution resolution =
                SettingsFragmentRegistry.resolve("chrome://settings/allSites?category=all_sites");
        assertNull(resolution.redirectUrl);
        assertEquals(AllSiteSettings.class, resolution.fragmentClass);

        resolution =
                SettingsFragmentRegistry.resolve(
                        "chrome://settings/siteSettings/category?category=camera");
        assertNull(resolution.redirectUrl);
        assertEquals(SingleCategorySettings.class, resolution.fragmentClass);
    }

    @Test
    public void testEveryRouteReachesAPageWithoutLooping() {
        // A fallback that itself redirects would leave the tab navigating in a circle, so walk
        // every registered route, with no arguments supplied, to a page that can be shown.
        for (String path : SettingsFragmentRegistry.sPathToRouteSpecMap.keySet()) {
            String url = "chrome://settings" + path;
            for (int hops = 0; ; hops++) {
                assertTrue(
                        "Redirect loop reached from chrome://settings" + path + " at " + url,
                        hops < 5);
                SettingsFragmentRegistry.Resolution resolution =
                        SettingsFragmentRegistry.resolve(url);
                if (resolution.redirectUrl == null) {
                    assertNotNull(url, resolution.fragmentClass);
                    break;
                }
                url = resolution.redirectUrl;
            }
        }
    }

    @Test
    public void testResolveShowsMainSettingsForUnroutedUrls() {
        // ChosenObjectSettings has no URL: it is identified by a serialized device descriptor.
        SettingsFragmentRegistry.Resolution resolution =
                SettingsFragmentRegistry.resolve("chrome://settings/chosenObject");
        assertNull(resolution.redirectUrl);
        assertEquals(MainSettings.class, resolution.fragmentClass);
        assertNull(
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/chosenObject"));

        resolution = SettingsFragmentRegistry.resolve("chrome://settings/notAPage");
        assertNull(resolution.redirectUrl);
        assertEquals(MainSettings.class, resolution.fragmentClass);
    }

    @Test
    public void testResolveWalksUpToTheClosestPageForUnknownPaths() {
        // A Url is user editable, so a mistyped or truncated path should land as close to what was
        // asked for as possible. Here the query separator was typed as "&", making the whole thing
        // one unknown path segment.
        assertRedirects(
                "chrome://settings/siteSettings/category&title=Location",
                "chrome://settings/siteSettings");

        // The redirect keeps the registered casing rather than the lowercased matching form.
        assertRedirects("chrome://settings/allsites/nonsense", "chrome://settings/allSites");

        // Several unknown segments still walk all the way back to a real page.
        assertRedirects("chrome://settings/siteSettings/a/b/c", "chrome://settings/siteSettings");
    }

    @Test
    public void testResolveRedirectsCategoryUrlMissingOnlyItsCategory() {
        // The title is decoration; the category is what names the page. Dropping the category
        // leaves nothing to show, so this falls back rather than landing on the settings root.
        assertRedirects(
                "chrome://settings/siteSettings/category?title=Location",
                "chrome://settings/siteSettings");
    }

    @Test
    public void testResolveRedirectsWhenRequiredArgumentIsEmpty() {
        // A Url is user editable, and trimming the value off a parameter is easier than trimming
        // the whole parameter. An empty value names nothing, so it is the same as being absent.
        assertRedirects("chrome://settings/siteDetails?site=", "chrome://settings/allSites");
        assertRedirects("chrome://settings/siteDetails?site", "chrome://settings/allSites");
        assertRedirects("chrome://settings/allSites/group?group=", "chrome://settings/allSites");
        assertRedirects(
                "chrome://settings/siteSettings/category?category=",
                "chrome://settings/siteSettings");
    }

    @Test
    public void testParseUrlArgumentsSkipsEmptyValues() {
        // The same rule applies to arguments that merely decorate a page: an empty title would
        // otherwise blank the toolbar instead of letting the page name itself.
        Bundle args =
                SettingsFragmentRegistry.parseUrlArguments(
                        "chrome://settings/siteSettings/category?category=camera&title=");
        assertFalse(args.containsKey(SingleCategorySettings.EXTRA_TITLE));
        assertEquals("camera", args.getString(SingleCategorySettings.EXTRA_CATEGORY));
    }

    @Test
    public void testResolveRedirectsUnknownCategories() {
        // A category this build does not have is not a page, and must not be mistaken for one
        // belonging to the other of the two category pages, which asserts on it.
        assertRedirects(
                "chrome://settings/siteSettings/category?category=unknown_nonsense",
                "chrome://settings/siteSettings");

        // On All sites the same string is only decoration: the page falls back to "All sites" for
        // any category it does not render, so it is shown rather than redirected. Redirecting
        // would hand the category to a page that cannot show it either, and it would come back.
        SettingsFragmentRegistry.Resolution resolution =
                SettingsFragmentRegistry.resolve(
                        "chrome://settings/allSites?category=unknown_nonsense");
        assertNull(resolution.redirectUrl);
        assertEquals(AllSiteSettings.class, resolution.fragmentClass);
    }

    @Test
    public void testResolveAppliesRouteDefaults() {
        SettingsFragmentRegistry.Resolution resolution =
                SettingsFragmentRegistry.resolve("chrome://settings/theme");
        assertEquals(ThemeSettingsFragment.class, resolution.fragmentClass);
        assertEquals(
                NightModeMetrics.ThemeSettingsEntry.SETTINGS,
                resolution.args.getInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY));
    }

    @Test
    public void testStorageAccessUrlRoundTrip() {
        // The arguments the site settings pages open this page with, which under Url navigation
        // are turned into a Url and parsed back out again.
        Bundle args = new Bundle();
        args.putSerializable(
                StorageAccessSubpageSettings.EXTRA_STORAGE_ACCESS_STATE,
                new Website(WebsiteAddress.create("https://example.com"), null));
        args.putBoolean(StorageAccessSubpageSettings.EXTRA_ALLOWED, true);

        String url =
                SettingsFragmentRegistry.createUrlForFragment(
                        StorageAccessSubpageSettings.class, args);
        assertEquals(
                "chrome://settings/storageAccess?allowed=true&site=https%3A%2F%2Fexample.com", url);

        SettingsFragmentRegistry.Resolution resolution = SettingsFragmentRegistry.resolve(url);
        assertNull(resolution.redirectUrl);
        assertEquals(StorageAccessSubpageSettings.class, resolution.fragmentClass);
        assertTrue(resolution.args.getBoolean(StorageAccessSubpageSettings.EXTRA_ALLOWED));
        assertNotNull(resolution.args.get(SingleWebsiteSettings.EXTRA_SITE_ADDRESS));

        // The permission state is half of what the page shows, so a Url without it has no page.
        assertRedirects(
                "chrome://settings/storageAccess?site=https://example.com",
                "chrome://settings/allSites");
    }

    private static void assertRedirects(String url, String expectedRedirectUrl) {
        SettingsFragmentRegistry.Resolution resolution = SettingsFragmentRegistry.resolve(url);
        assertEquals(url, expectedRedirectUrl, resolution.redirectUrl);
        assertNull(url, resolution.fragmentClass);
    }

    /** The page a settings path resolves to, or null if the path is not registered. */
    private static Class<? extends Fragment> fragmentClassForPath(String path) {
        return SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings" + path);
    }
}
