// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.content.Context;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Xml;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * A utility class for parsing XML preference files and extracting preference attributes into a list
 * of Bundle objects.
 */
@NullMarked
public class PreferenceParser {
    public static final String METADATA_HEADER = "header";
    public static final String METADATA_KEY = "key";
    public static final String METADATA_TITLE = "title";
    public static final String METADATA_SUMMARY = "summary";
    public static final String METADATA_FRAGMENT = "fragment";

    private static final String TAG = "PreferenceParser";

    /**
     * Parses a {@link androidx.preference.PreferenceScreen} XML resource to extract key attributes
     * for the Settings Search indexer.
     *
     * <p>This method performs a low-level traversal of the XML file. For each preference tag (e.g.,
     * {@code <Preference>}, {@code <SwitchPreferenceCompat>}), it extracts standard attributes like
     * {@code android:key}, {@code android:title}, {@code android:summary}, and {@code
     * android:fragment}.
     *
     * <p>It also tracks the title of the most recent {@code <PreferenceCategory>} to provide a
     * "header" for grouping related preferences in the search results.
     *
     * @param context The {@link Context} used to access application resources and resolve the
     *     values of the XML attributes (e.g., turning {@code @string/title} into "Title").
     * @param xmlResId The resource ID of the preference XML file to parse (e.g., {@code
     *     R.xml.main_preferences}).
     * @return A {@code List} of {@code Bundle} objects. Each bundle represents a single preference
     *     and contains its attributes, keyed by the {@code METADATA_*} constants in this class
     *     (e.g., {@link #METADATA_KEY}, {@link #METADATA_TITLE}).
     * @throws XmlPullParserException If an error occurs during the XML parsing process.
     * @throws IOException If the specified resource file cannot be opened or read.
     */
    public static List<Bundle> parsePreferences(Context context, int xmlResId)
            throws XmlPullParserException, IOException {
        List<Bundle> preferenceBundles = new ArrayList<>();
        XmlResourceParser parser = context.getResources().getXml(xmlResId);
        int eventType = parser.getEventType();
        // Name of a Preferences group. Used as a header in the search results.
        String header = null;
        while (eventType != XmlPullParser.END_DOCUMENT) {
            String tagName = parser.getName();
            if (eventType == XmlPullParser.START_TAG && !"PreferenceScreen".equals(tagName)) {
                AttributeSet attrs = Xml.asAttributeSet(parser);
                int[] androidAttrIds =
                        new int[] {
                            android.R.attr.title,
                            android.R.attr.key,
                            android.R.attr.summary,
                            android.R.attr.fragment
                        };
                TypedArray androidAttributes =
                        context.obtainStyledAttributes(attrs, androidAttrIds);
                try {
                    String title = androidAttributes.getString(0);
                    if ("PreferenceCategory".equals(tagName)) {
                        header = title;
                    } else {
                        String key = androidAttributes.getString(1);
                        String summary = androidAttributes.getString(2);
                        String fragment = androidAttributes.getString(3);

                        Bundle preferenceBundle = new Bundle();
                        preferenceBundle.putString(METADATA_HEADER, header);
                        preferenceBundle.putString(METADATA_KEY, key);
                        preferenceBundle.putString(METADATA_TITLE, title);
                        preferenceBundle.putString(METADATA_SUMMARY, summary);
                        preferenceBundle.putString(METADATA_FRAGMENT, fragment);
                        preferenceBundles.add(preferenceBundle);
                    }
                } finally {
                    androidAttributes.recycle();
                }
            }
            eventType = parser.next();
        }
        parser.close();
        return preferenceBundles;
    }

    /**
     * Parses a preference XML resource and adds its entries to the provided SettingsIndexData.
     *
     * @param context The application context.
     * @param xmlRes The XML resource to parse.
     * @param indexData The SettingsIndexData object to populate.
     * @param prefFragment The class name of the fragment this XML belongs to.
     */
    public static void parseAndPopulate(
            Context context, int xmlRes, SettingsIndexData indexData, String prefFragment) {
        List<Bundle> metadata;

        try {
            metadata = parsePreferences(context, xmlRes);
        } catch (IOException | XmlPullParserException e) {
            Log.e(TAG, "Failed to parse preference xml for populating index", e);
            return;
        }

        for (Bundle bundle : metadata) {
            String key = bundle.getString(METADATA_KEY);
            String title = bundle.getString(METADATA_TITLE);
            if (TextUtils.isEmpty(key)) continue;

            String uniqueId = createUniqueId(prefFragment, key);

            indexData.addEntry(
                    uniqueId,
                    new SettingsIndexData.Entry.Builder(uniqueId, key, title, prefFragment)
                            .setHeader(bundle.getString(METADATA_HEADER))
                            .setSummary(bundle.getString(METADATA_SUMMARY))
                            .setFragment(bundle.getString(METADATA_FRAGMENT))
                            .build());
        }
    }

    /**
     * Parses a preference XML to build a hierarchy of fragment headers for the search index. It
     * assigns a parent's header to its children and recursively calls itself for nested fragments.
     *
     * @param context The application context.
     * @param xmlRes The XML resource to parse.
     * @param parentFragmentName The class name of the fragment this XML belongs to.
     * @param indexData The SettingsIndexData object to populate with headers.
     * @param providerMap A map of all SearchIndexProviders to find children for recursion.
     * @param processedFragments A set of fragment names that have already been processed.
     */
    public static void parseAndRegisterHeaders(
            Context context,
            int xmlRes,
            String parentFragmentName,
            SettingsIndexData indexData,
            Map<String, SearchIndexProvider> providerMap,
            Set<String> processedFragments) {
        if (xmlRes == 0 || processedFragments.contains(parentFragmentName)) {
            return;
        }

        List<Bundle> metadata;

        try {
            metadata = PreferenceParser.parsePreferences(context, xmlRes);
        } catch (IOException | XmlPullParserException e) {
            Log.e(TAG, "Failed to parse preference xml for hierarchy registration", e);
            return;
        }

        processedFragments.add(parentFragmentName);

        for (Bundle bundle : metadata) {
            String parentKey = bundle.getString(PreferenceParser.METADATA_KEY);
            String childFragmentName = bundle.getString(PreferenceParser.METADATA_FRAGMENT);

            if (TextUtils.isEmpty(childFragmentName)) {
                continue;
            }

            assert !TextUtils.isEmpty(parentKey) : "Parent preference key is null/empty.";

            String uniqueParentId = createUniqueId(parentFragmentName, parentKey);

            indexData.addChildParentLink(childFragmentName, uniqueParentId);

            SearchIndexProvider childProvider = providerMap.get(childFragmentName);
            // TODO(adelm): Once all prefs have been registered, this should become an assert.
            if (childProvider != null) {
                childProvider.registerFragmentHeaders(
                        context, indexData, providerMap, processedFragments);
            }
        }
    }

    public static String createUniqueId(String parentFragmentName, String originalKey) {
        return parentFragmentName + "#" + originalKey;
    }
}
