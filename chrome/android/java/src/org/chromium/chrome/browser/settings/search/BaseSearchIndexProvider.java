// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.chromium.chrome.browser.settings.search.PreferenceParser.METADATA_FRAGMENT;
import static org.chromium.chrome.browser.settings.search.PreferenceParser.METADATA_HEADER;
import static org.chromium.chrome.browser.settings.search.PreferenceParser.METADATA_KEY;
import static org.chromium.chrome.browser.settings.search.PreferenceParser.METADATA_SUMMARY;
import static org.chromium.chrome.browser.settings.search.PreferenceParser.METADATA_TITLE;

import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.XmlRes;

import org.xmlpull.v1.XmlPullParserException;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.io.IOException;
import java.util.List;

/**
 * A basic SearchIndexProvider implementation that retrieves the preferences to index from xml
 * resource only.
 */
@NullMarked
public class BaseSearchIndexProvider implements SearchIndexProvider {

    private static final String TAG = "BaseSearchIndex";
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
        if (!ChromeFeatureList.sSearchInSettings.isEnabled()) {
            return;
        }

        if (indexData.isDisabledFragment(mPrefFragment) || mXmlRes == 0) {
            return;
        }

        try {
            List<Bundle> metadata = PreferenceParser.parsePreferences(context, mXmlRes);
            for (Bundle bundle : metadata) {
                String key = bundle.getString(METADATA_KEY);
                String title = bundle.getString(METADATA_TITLE);
                if (TextUtils.isEmpty(key) || TextUtils.isEmpty(title)) continue;

                indexData.addEntry(
                        key,
                        new SettingsIndexData.Entry(
                                key,
                                title,
                                bundle.getString(METADATA_HEADER),
                                bundle.getString(METADATA_SUMMARY),
                                bundle.getString(METADATA_FRAGMENT),
                                mPrefFragment));
            }
        } catch (IOException | XmlPullParserException e) {
            Log.e(TAG, "Failed to parse preference xml for getting controllers", e);
        }
    }
}
