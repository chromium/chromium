// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.search.BaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.search.PreferenceParser;
import org.chromium.components.browser_ui.settings.search.SearchIndexProvider;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.util.Map;

/**
 * A specialization of {@link BaseSearchIndexProvider} for Chrome-layer fragments that rely on the
 * profile.
 *
 * <p>It implements {@link ChromeSearchIndexProvider} to allow Profile access.
 */
@NullMarked
public class ChromeBaseSearchIndexProvider extends BaseSearchIndexProvider
        implements ChromeSearchIndexProvider {

    public ChromeBaseSearchIndexProvider(String fragmentName, int xmlRes) {
        super(fragmentName, xmlRes);
    }

    /**
     * Override of {@link SearchIndexProvider#initPreferenceXml()} that also accepts {@link Profile}
     * to determine the preference xml resource.
     *
     * @param context The {@link Context} used to access application resources.
     * @param profile The {@link Profile} used to obtain the preference xml resource.
     * @param indexData The central {@link SettingsIndexData} object to be populated.
     * @param providerMap Map of all registered providers, keyed by Fragment Class Name. Used to
     *     look up default extras for child fragments.
     */
    public void initPreferenceXml(
            Context context,
            Profile profile,
            SettingsIndexData indexData,
            Map<String, SearchIndexProvider> providerMap) {
        int xmlRes = getXmlRes(profile);
        if (xmlRes == 0) return;

        PreferenceParser.parseAndPopulate(
                context, xmlRes, indexData, getPrefFragmentName(), getExtras(), providerMap);
    }

    /**
     * Returns the XML resource defining the fragment's preference screen.
     *
     * @param profile The {@link Profile} used to obtain the preference xml resource.
     */
    public int getXmlRes(Profile profile) {
        return getXmlRes();
    }
}
