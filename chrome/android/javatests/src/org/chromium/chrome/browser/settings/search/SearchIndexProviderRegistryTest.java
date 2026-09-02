// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.junit.Assert.fail;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy_guide.PrivacyGuideFragment;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.components.browser_ui.settings.search.SearchIndexProvider;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Tests that the Settings Search Index can be built for all registered fragments without crashing,
 * with valid XML resources, and that all XML fragment links map to registered providers. This test
 * requires the native library to be loaded.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA,
    ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_BENEFITS_TOGGLE_TEXT,
    ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM,
    ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT,
    ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
    ChromeFeatureList.DETAILED_LANGUAGE_SETTINGS
})
public class SearchIndexProviderRegistryTest {

    /**
     * Fragments referenced by preference XMLs that are intentionally not indexed (e.g. multi-step
     * onboarding wizards or dynamic site-list pages).
     */
    private static final Set<String> UNINDEXED_FRAGMENTS_ALLOWLIST =
            Set.of(
                    AddressBarSettingsFragment.class.getName(),
                    AllSiteSettings.class.getName(),
                    PrivacyGuideFragment.class.getName());

    private Context mContext;
    private Profile mProfile;
    private SettingsIndexData mIndexData;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mContext = ContextUtils.getApplicationContext();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = ProfileManager.getLastUsedRegularProfile();
                    mIndexData = SettingsIndexData.createInstance();
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SettingsIndexData.reset();
                });
    }

    /** Verifies that all providers registered don't point to missing XML resources. */
    @Test
    @SmallTest
    public void testXmlResourcesExist() {
        List<SearchIndexProvider> providers = SearchIndexProviderRegistry.ALL_PROVIDERS;
        if (providers == null || providers.isEmpty()) {
            fail("SearchIndexProviderRegistry is empty!");
        }

        StringBuilder errorLog = new StringBuilder();
        int failureCount = 0;

        for (SearchIndexProvider provider : providers) {
            int xmlResId = provider.getXmlRes();
            if (xmlResId > 0) {
                try {
                    mContext.getResources().getXml(xmlResId);
                } catch (Resources.NotFoundException e) {
                    failureCount++;
                    errorLog.append(
                            String.format(
                                    "\n"
                                        + "Broken Link: Provider %s declared XML Resource ID (%d)"
                                        + " was not found.",
                                    provider.getClass().getSimpleName(), xmlResId));
                }
            }
        }
        if (failureCount > 0) {
            fail(errorLog.toString());
        }
    }

    /**
     * Verifies that the entire index building process (XML parsing and dynamic updates) completes
     * without throwing any exceptions for all registered providers.
     */
    @Test
    @SmallTest
    public void testFullIndexBuildsWithoutCrashing() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<SearchIndexProvider> providers = SearchIndexProviderRegistry.ALL_PROVIDERS;
                    if (providers == null || providers.isEmpty()) {
                        fail("SearchIndexProviderRegistry is empty! No indexes to test.");
                    }

                    try {
                        SettingsSearchCoordinator.buildIndexInternal(
                                mContext, mProfile, mIndexData);
                    } catch (Exception e) {
                        fail(
                                String.format(
                                        "Full index build failed: %s\n%s",
                                        e.getMessage(), Log.getStackTraceString(e)));
                    }
                });
    }

    /**
     * Verifies that every preference entry that declares a fragment target links to a fragment
     * whose provider is registered in SearchIndexProviderRegistry.
     */
    @Test
    @SmallTest
    public void testAllXmlFragmentLinksAreRegistered() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SettingsSearchCoordinator.buildIndexInternal(mContext, mProfile, mIndexData);

                    Set<String> registeredFragments = new HashSet<>();
                    for (SearchIndexProvider provider : SearchIndexProviderRegistry.ALL_PROVIDERS) {
                        registeredFragments.add(provider.getPrefFragmentName());
                    }

                    List<String> unindexedFragments = new ArrayList<>();
                    for (SettingsIndexData.Entry entry :
                            mIndexData.getEntriesForTesting().values()) {
                        if (!TextUtils.isEmpty(entry.fragment)
                                && !registeredFragments.contains(entry.fragment)
                                && !UNINDEXED_FRAGMENTS_ALLOWLIST.contains(entry.fragment)) {
                            unindexedFragments.add(
                                    String.format(
                                            "Entry '%s' (in %s) links to child fragment '%s' which"
                                                    + " has no SearchIndexProvider in"
                                                    + " SearchIndexProviderRegistry.",
                                            entry.id, entry.parentFragment, entry.fragment));
                        }
                    }

                    if (!unindexedFragments.isEmpty()) {
                        StringBuilder sb =
                                new StringBuilder(
                                        "Found preference entries linking to unindexed"
                                                + " fragments:\n");
                        for (String msg : unindexedFragments) {
                            sb.append("  - ").append(msg).append("\n");
                        }
                        fail(sb.toString());
                    }
                });
    }
}
