// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.junit.Assert.fail;

import android.content.Context;
import android.content.res.Resources;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.settings.search.SearchIndexProvider;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.List;

/**
 * Tests that the Settings Search Index can be built for all registered fragments without crashing.
 * This test requires the native library to be loaded.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA,
    ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
    ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_BMO,
    ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE,
    ChromeFeatureList.AUTOFILL_ENABLE_FLAT_RATE_CARD_BENEFITS_FROM_CURINOS,
    ChromeFeatureList.AUTOFILL_ENABLE_LOYALTY_CARDS_FILLING,
    ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_BENEFITS_TOGGLE_TEXT,
    ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM,
    ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT,
    ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
    ChromeFeatureList.DETAILED_LANGUAGE_SETTINGS,
    ChromeFeatureList.PLUS_ADDRESSES_ENABLED
})
public class SearchIndexProviderRegistryTest {

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Context sContext;
    private Profile mProfile;
    private SettingsIndexData mIndexData;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        sContext = sActivityTestRule.getActivity();
    }

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();

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
                    sContext.getResources().getXml(xmlResId);
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
                                sContext, mProfile, mIndexData);
                    } catch (Exception e) {
                        fail(
                                String.format(
                                        "Full index build failed: %s\n%s",
                                        e.getMessage(), Log.getStackTraceString(e)));
                    }
                });
    }
}
