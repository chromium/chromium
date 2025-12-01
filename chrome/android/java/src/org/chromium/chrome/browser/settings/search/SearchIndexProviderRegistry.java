// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/**
 * A central registry that holds the list of all SearchIndexProvider instances. This is the single
 * source of truth for the search indexing process.
 */
@NullMarked
public final class SearchIndexProviderRegistry {
    /**
     * The list of all providers that can be indexed for settings search.
     *
     * <p>When you create a new searchable PreferenceFragment, you must add its
     * SEARCH_INDEX_DATA_PROVIDER to this list.
     */
    public static final List<SearchIndexProvider> ALL_PROVIDERS =
            List.of(
                    // MainSettings always comes at the top. This fixes the order of the header
                    // groups in which they will be shown in the search results since the index
                    // uses LinkedHashMap internally.
                    org.chromium.chrome.browser.settings.MainSettings.SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.about_settings.AboutChromeSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.toolbar.adaptive.settings
                            .AdaptiveToolbarSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.contextualsearch.ContextualSearchSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy.settings.DoNotTrackSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.homepage.settings.HomepageSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.about_settings.LegalInformationSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.sync.settings.PersonalizeGoogleServicesSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy.secure_dns.SecureDnsSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.tasks.tab_management.TabArchiveSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.AdMeasurementFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.FledgeAllSitesFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.FledgeFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.FledgeLearnMoreFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.FledgeBlockedSitesFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.TopicsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.TopicsBlockedFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.TopicsManageFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER);
}
