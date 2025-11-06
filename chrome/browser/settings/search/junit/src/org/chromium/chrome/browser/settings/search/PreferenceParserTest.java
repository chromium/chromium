// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;

import java.util.List;

/** Unit tests for {@link PreferenceParser}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.SEARCH_IN_SETTINGS})
public class PreferenceParserTest {

    private Context mContext;
    private List<Bundle> mParsedMetadata;

    @Before
    public void setUp() throws Exception {
        mContext = ContextUtils.getApplicationContext();

        mParsedMetadata = PreferenceParser.parsePreferences(mContext, R.xml.main_preferences);
        assertNotNull("The parsed metadata should not be null.", mParsedMetadata);
    }

    @Test
    public void testParser_findsReasonableNumberOfPreferences() {
        assertTrue(
                "Should have parsed a reasonable number of preferences.",
                mParsedMetadata.size() > 10);
    }

    @Test
    public void testParser_correctlyParsesPreferenceInCategory() {
        @Nullable Bundle privacyBundle = findBundleByKey("privacy");
        assertNotNull("The 'privacy' preference should be found.", privacyBundle);

        assertEquals(
                mContext.getString(R.string.prefs_privacy_security),
                privacyBundle.getString(PreferenceParser.METADATA_TITLE));
        assertEquals(
                PrivacySettings.class.getName(),
                privacyBundle.getString(PreferenceParser.METADATA_FRAGMENT));
        assertEquals(
                "The header should be the title of its category.",
                mContext.getString(R.string.prefs_section_basics),
                privacyBundle.getString("header"));
    }

    @Test
    public void testParser_correctlyParsesPreferenceInDifferentCategory() {
        @Nullable Bundle tabsBundle = findBundleByKey("tabs");
        assertNotNull("The 'tabs' preference should be found.", tabsBundle);

        assertEquals(
                "The header should be the title of the 'Advanced' category.",
                mContext.getString(R.string.prefs_section_advanced),
                tabsBundle.getString("header"));
    }

    @Test
    public void testParser_handlesPreferenceWithNoFragment() {
        @Nullable Bundle notificationsBundle = findBundleByKey("notifications");
        assertNotNull("The 'notifications' preference should be found.", notificationsBundle);

        assertNull(
                "The 'notifications' preference should not have a fragment attribute.",
                notificationsBundle.getString(PreferenceParser.METADATA_FRAGMENT));
    }

    @Nullable
    private Bundle findBundleByKey(String key) {
        for (Bundle bundle : mParsedMetadata) {
            if (key.equals(bundle.getString(PreferenceParser.METADATA_KEY))) {
                return bundle;
            }
        }
        return null;
    }
}
