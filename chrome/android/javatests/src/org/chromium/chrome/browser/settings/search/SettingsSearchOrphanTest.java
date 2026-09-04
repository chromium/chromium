// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.junit.Assert.fail;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.settings.search.PreferenceParser;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.browser_ui.site_settings.CookieSettings;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Tests that building the Settings Search Index does not produce unexpected orphaned entries.
 * Validates preference graph reachability from child preferences back to MainSettings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID,
    ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA,
    ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_BENEFITS_TOGGLE_TEXT,
    ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM,
    ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT,
    ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
    ChromeFeatureList.DETAILED_LANGUAGE_SETTINGS
})
public class SettingsSearchOrphanTest {

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    // Pre-existing orphan bugs tracked in Buganizer that are pending dedicated fixes.
    private static final Set<String> KNOWN_ORPHAN_KEYS =
            Set.of(
                    // TODO(crbug.com/555170486): Fix CookieSettings orphan preference.
                    PreferenceParser.createUniqueId(CookieSettings.class.getName(), "allow_rws"));

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
        SettingsIndexData.reset();
    }

    /**
     * Verifies that when all standard feature flags are enabled, building the search index does not
     * produce any orphaned entries caused by missing XML links or broken ancestor paths.
     */
    @Test
    @SmallTest
    public void testNoUnexpectedOrphanedPreferences_signedOut() {
        assertNoUnexpectedOrphanedPreferences();
    }

    /**
     * Verifies that when a user is signed in, building the settings search index includes all Sync
     * and Account preferences (e.g. ManageSyncSettings) without producing any orphaned entries.
     */
    @Test
    @SmallTest
    public void testNoUnexpectedOrphanedPreferences_signedIn() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        assertNoUnexpectedOrphanedPreferences();
    }

    private enum OrphanReason {
        /**
         * Valid: All parent preferences were explicitly removed from the index (e.g. feature flag
         * off).
         */
        VALID_PARENT_REMOVAL,
        /**
         * Bug: Fragment is in registry, but no preference in the XML graph links to it via
         * android:fragment.
         */
        MISSING_PARENT_LINK,
        /** Bug: Parent preference is present in the index, but path finding to root failed. */
        BROKEN_ANCESTOR_PATH
    }

    private static class OrphanReport {
        public final SettingsIndexData.Entry entry;
        public final OrphanReason reason;
        public final String details;

        public OrphanReport(SettingsIndexData.Entry entry, OrphanReason reason, String details) {
            this.entry = entry;
            this.reason = reason;
            this.details = details;
        }

        @Override
        public String toString() {
            return String.format(
                    "[%s] Entry: %s (in %s) - %s", reason, entry.id, entry.parentFragment, details);
        }
    }

    private static OrphanReport diagnoseOrphan(
            SettingsIndexData.Entry entry,
            Map<String, SettingsIndexData.Entry> entries,
            Set<String> prunedEntryIds,
            Map<String, List<String>> childFragmentToParentKeys,
            String rootFragmentName) {
        List<String> parentKeys = childFragmentToParentKeys.get(entry.parentFragment);

        if (parentKeys == null || parentKeys.isEmpty()) {
            return new OrphanReport(
                    entry,
                    OrphanReason.MISSING_PARENT_LINK,
                    "No preference in the preference graph declares android:fragment=\""
                            + entry.parentFragment
                            + "\"");
        }

        List<String> activeParents = new ArrayList<>();
        for (String parentKey : parentKeys) {
            if (entries.containsKey(parentKey) || prunedEntryIds.contains(parentKey)) {
                activeParents.add(parentKey);
            }
        }

        if (!activeParents.isEmpty()) {
            return new OrphanReport(
                    entry,
                    OrphanReason.BROKEN_ANCESTOR_PATH,
                    "Parent preference(s) "
                            + activeParents
                            + " exist in index, but path to "
                            + rootFragmentName
                            + " is broken.");
        }

        return new OrphanReport(
                entry,
                OrphanReason.VALID_PARENT_REMOVAL,
                "All parent preference keys " + parentKeys + " were removed dynamically.");
    }

    private void assertNoUnexpectedOrphanedPreferences() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<SettingsIndexData.Entry> prunedEntries =
                            SettingsSearchCoordinator.buildIndexInternal(
                                    mContext, mProfile, mIndexData);

                    Set<String> prunedEntryIds = new HashSet<>();
                    for (SettingsIndexData.Entry pruned : prunedEntries) {
                        prunedEntryIds.add(pruned.id);
                    }

                    Map<String, SettingsIndexData.Entry> entries =
                            mIndexData.getEntriesForTesting();
                    Map<String, List<String>> childFragmentToParentKeys =
                            mIndexData.getChildFragmentToParentKeysForTesting();

                    List<OrphanReport> unexpectedBugs = new ArrayList<>();
                    for (SettingsIndexData.Entry pruned : prunedEntries) {
                        if (KNOWN_ORPHAN_KEYS.contains(pruned.id)) {
                            continue;
                        }
                        OrphanReport report =
                                diagnoseOrphan(
                                        pruned,
                                        entries,
                                        prunedEntryIds,
                                        childFragmentToParentKeys,
                                        MainSettings.class.getName());

                        if (report.reason != OrphanReason.VALID_PARENT_REMOVAL) {
                            unexpectedBugs.add(report);
                        }
                    }

                    if (!unexpectedBugs.isEmpty()) {
                        StringBuilder sb =
                                new StringBuilder(
                                        "Found Settings Search indexing wiring bugs (orphaned"
                                                + " entries during index resolution):\n");
                        for (OrphanReport bug : unexpectedBugs) {
                            sb.append("  - ").append(bug.toString()).append("\n");
                        }
                        fail(sb.toString());
                    }
                });
    }
}
