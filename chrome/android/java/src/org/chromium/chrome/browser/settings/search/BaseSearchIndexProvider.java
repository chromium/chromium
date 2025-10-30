// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.content.Context;

import androidx.annotation.XmlRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * A basic SearchIndexProvider implementation that retrieves the preferences to index from xml
 * resource only.
 */
@NullMarked
public class BaseSearchIndexProvider implements SearchIndexProvider {

    private final int mXmlRes;
    private final String mPrefFragment;

    /**
     * Constructor for Fragment without XML resource.
     *
     * @param prefFragment {@link PreferenceFragment} owning this {@link SearchIndexProvider}.
     */
    public BaseSearchIndexProvider(String prefFragment) {
        this(prefFragment, 0);
    }

    /**
     * Constructor for Fragment.
     *
     * @param prefFragment {@link PreferenceFragment} owning this {@link SearchIndexProvider}.
     * @param xmlRes Preference XML resource.
     */
    public BaseSearchIndexProvider(String prefFragment, @XmlRes int xmlRes) {
        mPrefFragment = prefFragment;
        mXmlRes = xmlRes;
    }

    /** Returns the name of the associated {@link PreferenceFragment}. */
    public String getPrefFragmentName() {
        return mPrefFragment;
    }

    @Override
    public void initPreferenceXml(Context context, SettingsIndexData indexData) {
        if (ChromeFeatureList.sSearchInSettings.isEnabled() && mXmlRes != 0) {
            PreferenceParser.parseAndPopulate(context, mXmlRes, indexData, mPrefFragment);
        }
    }
}
