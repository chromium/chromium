// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.content.Context;

import androidx.annotation.XmlRes;

import org.chromium.build.annotations.NullMarked;

import java.util.Map;
import java.util.Set;

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

    /** Returns the Preference XML resource. */
    public @XmlRes int getXmlRes() {
        return mXmlRes;
    }

    /**
     * Returns the unique id for a child pref.
     *
     * @param childPrefName The name of the child pref.
     * @return The unique id for that child pref.
     */
    public String getUniqueId(String childPrefName) {
        return PreferenceParser.createUniqueId(mPrefFragment, childPrefName);
    }

    /** Returns the name of the associated {@link PreferenceFragment}. */
    @Override
    public String getPrefFragmentName() {
        return mPrefFragment;
    }

    @Override
    public void registerFragmentHeaders(
            Context context,
            SettingsIndexData indexData,
            Map<String, SearchIndexProvider> providerMap,
            Set<String> processedFragments) {
        PreferenceParser.parseAndRegisterHeaders(
                context, mXmlRes, mPrefFragment, indexData, providerMap, processedFragments);
    }

    @Override
    public void initPreferenceXml(Context context, SettingsIndexData indexData) {
        if (mXmlRes != 0) {
            PreferenceParser.parseAndPopulate(context, mXmlRes, indexData, mPrefFragment);
        }
    }
}
