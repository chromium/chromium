// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.hamcrest.CoreMatchers.endsWith;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.startsWith;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.net.Uri;
import android.text.TextUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;

/** Tests the {@link RelatedSearchesStamp} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class RelatedSearchesStampTest {
    /** The "stamp" encodes the experiment and its processing history, and is built from these. */
    private static final String RELATED_SEARCHES_LANGUAGE_RESTRICTION = "l";

    /**
     * The stamps to use for various experiment configurations. Note that users still may need
     * the ability to send everything in order to keep the experiment populations balanced.
     */
    private static final String EXPECTED_DEFAULT_STAMP = "1Rs";
    private static final String EXPECTED_DEFAULT_STAMP_LANGUAGE_RESTRICTED = "1Rsl";
    private static final String EXPECTED_DEFAULT_STAMP_ALL_LANGUAGE = "1Rsa";

    /** The stamp CGI parameter. */
    private static final String RELATED_SEARCHES_STAMP_PARAM = "ctxsl_rs";

    private static final String EXPECTED_POSITION_ENDING = "Up";
    private static final Uri SAMPLE_URI =
            Uri.parse("https://www.google.com/search?q=query&ctxsl_rs=" + EXPECTED_DEFAULT_STAMP);

    private static final String ENGLISH = "en";
    private static final String SPANISH = "es";
    private static final String GERMAN = "de";

    @Mock private Profile mProfile;

    private ContextualSearchPolicy mPolicy;
    private FeatureList.TestValues mFeatureListValues;

    /** Our instance under test. */
    private RelatedSearchesStamp mStamp;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mFeatureListValues = new FeatureList.TestValues();
        FeatureList.setTestValues(mFeatureListValues);
        mPolicy = new ContextualSearchPolicy(mProfile, null, null);
        mStamp = new RelatedSearchesStamp(mPolicy);
    }

    // ====================================================================================
    // Helper methods
    // ====================================================================================

    /** Sets whether the user has allowed sending content (has done the opt-in). */
    private void setCanSendContent(boolean canSend) {
        mPolicy.overrideDecidedStateForTesting(canSend);
    }

    /**
     * Sets whether the user has allowed sending the URL (has enabled "Make search and browsing
     * better").
     */
    private void setCanSendUrl(boolean canSend) {
        mPolicy.overrideAllowSendingPageUrlForTesting(canSend);
    }

    /**
     * Sets whether the config specifies if the content can be any language to get any Related
     * Searches.
     */
    private void setSupportAllLanguage(boolean support) {
        mFeatureListValues.addFeatureFlagOverride(
                ChromeFeatureList.RELATED_SEARCHES_ALL_LANGUAGE, support);
    }

    /** Sets whether the Related Searches switch is enabled. */
    private void setRelatedSearchesSwitch(boolean enable) {
        mFeatureListValues.addFeatureFlagOverride(
                ChromeFeatureList.RELATED_SEARCHES_SWITCH, enable);
    }

    /** Sets the standard config setup that we're using for Related Searches experiments. */
    private void setStandardExperimentRequirements() {
        // For experimentation we currently require all users have all the permissions
        // for all experiment arms, and we restrict the language to English-only.
        setSupportAllLanguage(false);
    }

    /** Sets a standard config setup for the default Related Searches launch configuration. */
    private void setStandardDefaultLaunchConfiguration() {
        setStandardExperimentRequirements();
        setCanSendUrl(true);
        setCanSendContent(true);
        setRelatedSearchesSwitch(true);
    }

    // ====================================================================================
    // TESTS
    // ====================================================================================

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetRelatedSearchesStampForUnspecifiedExperiments() {
        setStandardDefaultLaunchConfiguration();
        // When there's no stamp in the config we expect to build a version-1 stamp.
        // This can happen when flags are flipped manually.
        assertThat(
                "A config without any stamp should default to language restricted, but German is "
                        + "still generating suggestions!",
                mStamp.getRelatedSearchesStamp(GERMAN),
                is(""));
        assertThat(
                "A config without any stamp should default to language restricted with both kinds "
                        + "of suggestions, but is not!",
                mStamp.getRelatedSearchesStamp(ENGLISH),
                is(EXPECTED_DEFAULT_STAMP + RELATED_SEARCHES_LANGUAGE_RESTRICTION));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampNotUrlQualified() {
        setStandardDefaultLaunchConfiguration();
        setCanSendUrl(false);
        assertTrue(
                "Users that have not enabled sending a URL are still generating Related Searches "
                        + "on a URL experiment!",
                TextUtils.isEmpty(mStamp.getRelatedSearchesStamp(ENGLISH)));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampNotContentQualified() {
        setStandardDefaultLaunchConfiguration();
        setCanSendContent(false);
        assertTrue(
                "Users that have not enabled sending page content are still generating Related "
                        + "Searches on a content-only experiment!",
                TextUtils.isEmpty(mStamp.getRelatedSearchesStamp(ENGLISH)));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampNotLanguageQualified() {
        setStandardDefaultLaunchConfiguration();
        assertFalse(
                "A standard experiment with both inputs is not generating Related Searches for "
                        + "English, but should!",
                TextUtils.isEmpty(mStamp.getRelatedSearchesStamp(ENGLISH)));
        assertTrue(
                "A standard experiment with both inputs is generating Related Searches for German, "
                        + "but should not!",
                TextUtils.isEmpty(mStamp.getRelatedSearchesStamp(GERMAN)));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampLanguageRestricted() {
        setStandardDefaultLaunchConfiguration();
        assertThat(
                "A launch configuration with multiple languages is not generating the expected "
                        + "processing stamp for English!",
                mStamp.getRelatedSearchesStamp(ENGLISH),
                is(EXPECTED_DEFAULT_STAMP_LANGUAGE_RESTRICTED));
        assertThat(
                "A launch configuration with multiple languages is generating Related Searches "
                        + "when it should be language restricted for German!",
                mStamp.getRelatedSearchesStamp(GERMAN),
                is(""));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampLanguageRestrictedForAllLanguages() {
        setStandardDefaultLaunchConfiguration();
        setSupportAllLanguage(true);
        assertThat(
                "A launch configuration with all languages support is not generating the expected "
                        + "processing stamp for English!",
                mStamp.getRelatedSearchesStamp(ENGLISH),
                is(EXPECTED_DEFAULT_STAMP_ALL_LANGUAGE));
        assertThat(
                "A launch configuration with all languages support is not generating the expected "
                        + "processing stamp for Spanish!",
                mStamp.getRelatedSearchesStamp(SPANISH),
                is(EXPECTED_DEFAULT_STAMP_ALL_LANGUAGE));
        assertThat(
                "A launch configuration with all languages support is not generating the expected "
                        + "processing stamp for German!",
                mStamp.getRelatedSearchesStamp(GERMAN),
                is(EXPECTED_DEFAULT_STAMP_ALL_LANGUAGE));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testUpdateUriForSelectedPosition() {
        setStandardDefaultLaunchConfiguration();
        Uri updatedUri = RelatedSearchesStamp.updateUriForSuggestionPosition(SAMPLE_URI, 3);
        String stampParam = updatedUri.getQueryParameter(RELATED_SEARCHES_STAMP_PARAM);
        assertThat(
                "Appending the UI position of a chosen Related Searches suggestion doesn't have "
                        + "the expected prefix!",
                stampParam,
                startsWith(EXPECTED_DEFAULT_STAMP));
        assertThat(
                "Appending the UI position of a chosen Related Searches suggestion doesn't have "
                        + "the expected psotion index!",
                stampParam,
                endsWith(EXPECTED_POSITION_ENDING + "3"));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testReplaceQueryParam() {
        Uri updatedQuery = RelatedSearchesStamp.replaceQueryParam(SAMPLE_URI, "q", "newQuery");
        assertTrue(
                "Replacing a query parameter is not producing the expected CGI param!",
                updatedQuery.toString().contains("?q=newQuery"));
        String doubleUpdatedQuery =
                RelatedSearchesStamp.replaceQueryParam(updatedQuery, "q", "newerQuery").toString();
        assertTrue(
                "Replacing a query parameter with a newer one is not producing the expected CGI "
                        + "param!",
                doubleUpdatedQuery.contains("?q=newerQuery"));
        assertFalse(
                "Replacing a query parameter appears to fail to remove the original param!",
                doubleUpdatedQuery.contains("newQuery"));
        // Test removal
        String uriWithNoQ =
                RelatedSearchesStamp.replaceQueryParam(SAMPLE_URI, "q", null).toString();
        assertFalse("Removing a query parameter isn't working!", uriWithNoQ.contains("q"));
        // Test replacement of a param that doesn't exist
        String shouldBeUnchanged =
                RelatedSearchesStamp.replaceQueryParam(SAMPLE_URI, "qqq", "newQuery").toString();
        assertTrue(
                "Replacing a non-existing parameter is removing an existing one!",
                shouldBeUnchanged.contains("?q=query"));
        assertFalse(
                "Replacing a non-existing parameter is adding the new parameter anyway!",
                shouldBeUnchanged.contains("qqq"));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testRelatedSearchSwitchIsDisabled() {
        setStandardDefaultLaunchConfiguration();
        setRelatedSearchesSwitch(false);
        assertThat(
                "related searches should be disabled!",
                mStamp.getRelatedSearchesStamp(GERMAN),
                is(""));
        assertThat(
                "related searches should be disabled!",
                mStamp.getRelatedSearchesStamp(ENGLISH),
                is(""));
    }
}
