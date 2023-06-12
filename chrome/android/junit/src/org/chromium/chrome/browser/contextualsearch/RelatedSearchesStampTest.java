// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.hamcrest.CoreMatchers.anyOf;
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
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Tests the {@link RelatedSearchesStamp} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {RelatedSearchesStampTest.ShadowChromeFeatureList.class,
                RelatedSearchesStampTest.ShadowContextualSearchFieldTrial.class})
public class RelatedSearchesStampTest {
    /** Here are the requirements that are set up in the configuration. */
    private static final String RELATED_SEARCHES_NEEDS_URL = "needs_url";
    private static final String RELATED_SEARCHES_NEEDS_CONTENT = "needs_content";
    private static final String RELATED_SEARCHES_LANGUAGE_ALLOWLIST = "language_allowlist";
    private static final String RELATED_SEARCHES_ALL_LANGUAGES = "all_languages";

    /** The "stamp" encodes the experiment and its processing history, and is built from these. */
    private static final String RELATED_SEARCHES_STAMP_VERSION = "1";
    private static final String RELATED_SEARCHES_EXPERIMENT_RECIPE_STAGE = "R";
    private static final String RELATED_SEARCHES_NO_EXPERIMENT = "n";
    private static final String RELATED_SEARCHES_URL_EXPERIMENT = "u";
    private static final String RELATED_SEARCHES_CONTENT_EXPERIMENT = "c";
    private static final String RELATED_SEARCHES_BOTH_EXPERIMENT = "b";
    private static final String RELATED_SEARCHES_LANGUAGE_RESTRICTION = "l";
    private static final String RELATED_SEARCHES_USER_INTERACTION = "U";
    private static final String RELATED_SEARCHES_SELECTED_POSITION = "p";

    /**
     * The stamps to use for various experiment configurations. Note that users still may need
     * the ability to send everything in order to keep the experiment populations balanced.
     */
    private static final String CONFIG_STAMP_URL_ONLY = RELATED_SEARCHES_STAMP_VERSION
            + RELATED_SEARCHES_EXPERIMENT_RECIPE_STAGE + RELATED_SEARCHES_URL_EXPERIMENT;
    private static final String CONFIG_STAMP_CONTENT_ONLY = RELATED_SEARCHES_STAMP_VERSION
            + RELATED_SEARCHES_EXPERIMENT_RECIPE_STAGE + RELATED_SEARCHES_CONTENT_EXPERIMENT;
    private static final String CONFIG_STAMP_BOTH = RELATED_SEARCHES_STAMP_VERSION
            + RELATED_SEARCHES_EXPERIMENT_RECIPE_STAGE + RELATED_SEARCHES_BOTH_EXPERIMENT;
    private static final String EXPECTED_DEFAULT_STAMP = RELATED_SEARCHES_STAMP_VERSION
            + RELATED_SEARCHES_EXPERIMENT_RECIPE_STAGE + RELATED_SEARCHES_NO_EXPERIMENT;
    private static final String EXPECTED_DEFAULT_STAMP_LANGUAGE_RESTRICTED =
            EXPECTED_DEFAULT_STAMP + RELATED_SEARCHES_LANGUAGE_RESTRICTION;

    /** The stamp CGI parameter. */
    private static final String RELATED_SEARCHES_STAMP_PARAM = "ctxsl_rs";
    private static final String EXPECTED_POSITION_ENDING =
            RELATED_SEARCHES_USER_INTERACTION + RELATED_SEARCHES_SELECTED_POSITION;
    private static final Uri SAMPLE_URI =
            Uri.parse("https://www.google.com/search?q=query&ctxsl_rs=" + EXPECTED_DEFAULT_STAMP);

    private static final String ENGLISH = "en";
    private static final String SPANISH = "es";
    private static final String GERMAN = "de";
    private static final String ENGLISH_AND_SPANISH = ENGLISH + "," + SPANISH;

    /**
     * Values to return from Shadows.
     * These must be static because the original and shadow methods are static.
     */
    private static String sRelatedSearchesLanguageAllowlist;
    // These need to be Boolean instead of boolean so they can be static.
    private static Boolean sRelatedSearchesNeedsUrl;
    private static Boolean sRelatedSearchesNeedsContent;
    private static Boolean sRelatedSearchesSupportAllLanguages;
    private static String sRelatedSearchesExperimentConfigurationStamp;

    //=========================================================================================
    // Shadow classes are used to override static methods to enable them to return test values.
    //=========================================================================================

    /**
     * Shadows the ChromeFeatureList class.
     * Note that isEnabled must be shadowed and that all methods here need to be protected.
     * Also all method signatures must match or a silent failure to shadow will occur.
     */
    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        @Implementation
        protected static boolean getFieldTrialParamByFeatureAsBoolean(
                String featureName, String paramName, boolean defaultValue) {
            assertThat(featureName, is(ChromeFeatureList.RELATED_SEARCHES));
            assertThat(paramName,
                    anyOf(is(ContextualSearchFieldTrial.RELATED_SEARCHES_NEEDS_URL_PARAM_NAME),
                            is(ContextualSearchFieldTrial
                                            .RELATED_SEARCHES_NEEDS_CONTENT_PARAM_NAME)));
            if (paramName.equals(
                        ContextualSearchFieldTrial.RELATED_SEARCHES_NEEDS_URL_PARAM_NAME)) {
                return sRelatedSearchesNeedsUrl == null ? defaultValue : sRelatedSearchesNeedsUrl;
            } else if (paramName.equals(ContextualSearchFieldTrial
                                                .RELATED_SEARCHES_NEEDS_CONTENT_PARAM_NAME)) {
                return sRelatedSearchesNeedsContent == null ? defaultValue
                                                            : sRelatedSearchesNeedsContent;
            }
            return defaultValue;
        }

        @Implementation
        protected static boolean isEnabled(String featureName) {
            return true;
        }
    }

    /**
     * Shadows the ContextualSearchFieldTrial class.
     * Note that all methods here need to be protected regardless of the access of the
     * method being shadowed.
     */
    @Implements(ContextualSearchFieldTrial.class)
    static class ShadowContextualSearchFieldTrial {
        @Implementation
        protected static String getRelatedSearchesParam(String paramName) {
            // The code under test only calls this method for this particular param.
            assertThat(paramName, is(RELATED_SEARCHES_LANGUAGE_ALLOWLIST));
            return sRelatedSearchesLanguageAllowlist;
        }

        @Implementation
        protected static String getRelatedSearchesExperimentConfigurationStamp() {
            return sRelatedSearchesExperimentConfigurationStamp;
        }

        @Implementation
        protected static boolean isRelatedSearchesParamEnabled(String relatedSearchesParamName) {
            if (relatedSearchesParamName.equals(RELATED_SEARCHES_NEEDS_URL)) {
                return sRelatedSearchesNeedsUrl;
            } else if (relatedSearchesParamName.equals(RELATED_SEARCHES_NEEDS_CONTENT)) {
                return sRelatedSearchesNeedsContent;
            } else {
                assertThat(relatedSearchesParamName, is(RELATED_SEARCHES_ALL_LANGUAGES));
                return sRelatedSearchesSupportAllLanguages;
            }
        }
    }

    private ContextualSearchPolicy mPolicy;

    /** Our instance under test. */
    private RelatedSearchesStamp mStamp;

    @Before
    public void setup() {
        resetShadows();
        mPolicy = new ContextualSearchPolicy(null, null);
        mStamp = new RelatedSearchesStamp(mPolicy);
        mStamp.disableDefaultAllowedLanguagesForTesting(true);
    }

    //====================================================================================
    // Helper methods
    //====================================================================================

    /** Resets all of the static return values for all our shadow classes. */
    private void resetShadows() {
        sRelatedSearchesLanguageAllowlist = "";
        sRelatedSearchesNeedsUrl = null;
        sRelatedSearchesNeedsContent = null;
        sRelatedSearchesSupportAllLanguages = null;
        sRelatedSearchesExperimentConfigurationStamp = null;
    }

    /** Sets whether the user has allowed sending content (has done the opt-in). */
    private void setCanSendContent(boolean canSend) {
        mPolicy.overrideDecidedStateForTesting(canSend);
    }

    /**
     * Sets whether the config specifies that the user needs permissions for sending content in
     * order to get any Related Searches.
     */
    private void setNeedsContent(boolean needsContent) {
        sRelatedSearchesNeedsContent = needsContent;
    }

    /**
     * Sets whether the user has allowed sending the URL (has enabled "Make search and browsing
     * better").
     */
    private void setCanSendUrl(boolean canSend) {
        mPolicy.overrideAllowSendingPageUrlForTesting(canSend);
    }

    /**
     * Sets whether the config specifies that the user needs permissions for sending the URL in
     * order to get any Related Searches.
     */
    private void setNeedsUrl(boolean needsUrl) {
        sRelatedSearchesNeedsUrl = needsUrl;
    }

    /**
     * Sets whether the config specifies if the content can be any language to get any Related
     * Searches.
     */
    private void setSupportAllLanguage(boolean support) {
        sRelatedSearchesSupportAllLanguages = support;
    }

    /**
     * Sets whether the config specifies that the content must be in English (or some list of
     * allowed languages) in order to get any Related Searches.
     */
    private void setLanguageAllowlist(String commaSeparatedLanguages) {
        sRelatedSearchesLanguageAllowlist = commaSeparatedLanguages;
    }

    /**
     * Clears the list of allowed languages so there's no language restriction to get Related
     * Searches.
     */
    private void clearLanguageAllowlist() {
        sRelatedSearchesLanguageAllowlist = "";
    }

    /** Sets the base stamp that the config specifies for this Related Searches experiment arm. */
    private void setRelatedSearchesExperimentConfigurationStamp(String stampFromConfig) {
        sRelatedSearchesExperimentConfigurationStamp = stampFromConfig;
    }

    /** Sets the standard config setup that we're using for Related Searches experiments. */
    private void setStandardExperimentRequirements() {
        // For experimentation we currently require all users have all the permissions
        // for all experiment arms, and we restrict the language to English-only.
        setNeedsUrl(true);
        setNeedsContent(true);
        setSupportAllLanguage(false);
        setLanguageAllowlist(ENGLISH);
    }

    /**
     * Sets a standard config setup for the default Related Searches launch configuration.
     */
    private void setStandardDefaultLaunchConfiguration() {
        setStandardLaunchConfiguration("");
    }

    /**
     * Sets a standard config setup for a particular Related Searches launch configuration.
     * @param stampFromConfig The base stamp just as we expect it to be set in the experiment
     *         config.
     */
    private void setStandardLaunchConfiguration(String stampFromConfig) {
        setStandardExperimentConfiguration(stampFromConfig);
        clearLanguageAllowlist();
    }

    /**
     * Sets a standard config setup for a particular Related Searches experiment arm.
     * @param stampFromConfig The base stamp just as we expect it to be set in the experiment
     *         config.
     */
    private void setStandardExperimentConfiguration(String stampFromConfig) {
        setStandardExperimentRequirements();
        setCanSendUrl(true);
        setCanSendContent(true);
        setRelatedSearchesExperimentConfigurationStamp(stampFromConfig);
    }

    //====================================================================================
    // TESTS
    //====================================================================================

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetRelatedSearchesStampExpectedLaunchConfiguration() {
        setStandardDefaultLaunchConfiguration();
        assertThat("Non English pages are not generating the expected stamp to track usage for the "
                        + "expected launch configuration!",
                mStamp.getRelatedSearchesStamp(GERMAN), is(EXPECTED_DEFAULT_STAMP));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampUrlOnlyLaunchConfig() {
        setStandardLaunchConfiguration(CONFIG_STAMP_URL_ONLY);
        setNeedsContent(false);
        setCanSendContent(false);
        assertThat("Non English pages are not generating the expected stamp to track usage for a "
                        + "URL-only launch configuration!",
                mStamp.getRelatedSearchesStamp(GERMAN), is(CONFIG_STAMP_URL_ONLY));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampContentOnlyLaunchConfig() {
        setStandardLaunchConfiguration(CONFIG_STAMP_CONTENT_ONLY);
        setNeedsUrl(false);
        setCanSendUrl(false);
        assertThat("Non English pages are not generating the expected stamp to track usage for a "
                        + "content-only launch configuration!",
                mStamp.getRelatedSearchesStamp(GERMAN), is(CONFIG_STAMP_CONTENT_ONLY));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampExplicitLaunchConfig() {
        setStandardLaunchConfiguration(CONFIG_STAMP_BOTH);
        assertThat("Non English pages are not generating the expected stamp to track usage for a "
                        + "combined URL and content launch configuration!",
                mStamp.getRelatedSearchesStamp(GERMAN), is(CONFIG_STAMP_BOTH));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampExplicitLanguageRestrictedLaunchConfig() {
        setStandardLaunchConfiguration(CONFIG_STAMP_BOTH);
        setLanguageAllowlist(ENGLISH_AND_SPANISH);
        assertThat("English pages are not generated the expected stamp to track usage with a "
                        + "multi-language allow-list!",
                mStamp.getRelatedSearchesStamp(ENGLISH),
                is(CONFIG_STAMP_BOTH + RELATED_SEARCHES_LANGUAGE_RESTRICTION));
        assertThat("Spanish pages are not generated the expected stamp to track usage with a "
                        + "multi-language allow-list!",
                mStamp.getRelatedSearchesStamp(SPANISH),
                is(CONFIG_STAMP_BOTH + RELATED_SEARCHES_LANGUAGE_RESTRICTION));
        assertThat("German pages are generating a Related Searches tracking stamp even though the "
                        + "multi-language allow-list doesn't include German!",
                mStamp.getRelatedSearchesStamp(GERMAN), is(""));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetRelatedSearchesStampForUrlExperiments() {
        setStandardExperimentConfiguration(CONFIG_STAMP_URL_ONLY);
        assertThat("German pages are generating a Related Searches tracking stamp on a URL-only "
                        + "experiment even though the standard English allow-list should exclude "
                        + "German!",
                mStamp.getRelatedSearchesStamp(GERMAN), is(""));
        assertThat(
                "English pages are not generated the expected stamp to track usage on a URL-only "
                        + "experiment with a standard allow-list!",
                mStamp.getRelatedSearchesStamp(ENGLISH),
                is(CONFIG_STAMP_URL_ONLY + RELATED_SEARCHES_LANGUAGE_RESTRICTION));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetRelatedSearchesStampForContentExperiments() {
        setStandardExperimentConfiguration(CONFIG_STAMP_CONTENT_ONLY);
        assertThat(mStamp.getRelatedSearchesStamp(GERMAN), is(""));
        assertThat(mStamp.getRelatedSearchesStamp(ENGLISH),
                is(CONFIG_STAMP_CONTENT_ONLY + RELATED_SEARCHES_LANGUAGE_RESTRICTION));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetRelatedSearchesStampForBothKindsOfSuggestionsExperiments() {
        setStandardExperimentConfiguration(CONFIG_STAMP_BOTH);
        assertThat("German pages are generating a Related Searches stamp when an experiment should "
                        + "be language restricted!",
                mStamp.getRelatedSearchesStamp(GERMAN), is(""));
        assertThat("An English page is not generated the expected stamp for an experiment for both "
                        + "kinds of input!",
                mStamp.getRelatedSearchesStamp(ENGLISH),
                is(CONFIG_STAMP_BOTH + RELATED_SEARCHES_LANGUAGE_RESTRICTION));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetRelatedSearchesStampForUnspecifiedExperiments() {
        setStandardExperimentConfiguration("");
        // When there's no stamp in the config we expect to build a version-1 stamp.
        // This can happen when flags are flipped manually.
        assertThat(
                "A config without any stamp should default to language restricted, but German is "
                        + "still generating suggestions!",
                mStamp.getRelatedSearchesStamp(GERMAN), is(""));
        assertThat(
                "A config without any stamp should default to language restricted with both kinds "
                        + "of suggestions, but is not!",
                mStamp.getRelatedSearchesStamp(ENGLISH),
                is(EXPECTED_DEFAULT_STAMP + RELATED_SEARCHES_LANGUAGE_RESTRICTION));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampNotUrlQualified() {
        setStandardExperimentConfiguration(CONFIG_STAMP_URL_ONLY);
        setCanSendUrl(false);
        assertTrue(
                "Users that have not enabled sending a URL are still generating Related Searches "
                        + "on a URL experiment!",
                TextUtils.isEmpty(mStamp.getRelatedSearchesStamp(ENGLISH)));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampNotContentQualified() {
        setStandardExperimentConfiguration(CONFIG_STAMP_CONTENT_ONLY);
        setCanSendContent(false);
        assertTrue("Users that have not enabled sending page content are still generating Related "
                        + "Searches on a content-only experiment!",
                TextUtils.isEmpty(mStamp.getRelatedSearchesStamp(ENGLISH)));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampNotLanguageQualified() {
        setStandardExperimentConfiguration(CONFIG_STAMP_BOTH);
        assertFalse("A standard experiment with both inputs is not generating Related Searches for "
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
        setLanguageAllowlist(ENGLISH_AND_SPANISH);
        assertThat("A launch configuration with multiple languages is not generating the expected "
                        + "processing stamp for English!",
                mStamp.getRelatedSearchesStamp(ENGLISH),
                is(EXPECTED_DEFAULT_STAMP_LANGUAGE_RESTRICTED));
        assertThat("A launch configuration with multiple languages is not generating the expected "
                        + "processing stamp for Spanish!",
                mStamp.getRelatedSearchesStamp(SPANISH),
                is(EXPECTED_DEFAULT_STAMP_LANGUAGE_RESTRICTED));
        assertThat("A launch configuration with multiple languages is generating Related Searches "
                        + "when it should be language restricted for German!",
                mStamp.getRelatedSearchesStamp(GERMAN), is(""));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testGetStampLanguageRestrictedForAllLanguages() {
        setStandardDefaultLaunchConfiguration();
        setSupportAllLanguage(true);
        assertThat(
                "A launch configuration with all languages support is not generating the expected "
                        + "processing stamp for English!",
                mStamp.getRelatedSearchesStamp(ENGLISH), is(EXPECTED_DEFAULT_STAMP));
        assertThat(
                "A launch configuration with all languages support is not generating the expected "
                        + "processing stamp for Spanish!",
                mStamp.getRelatedSearchesStamp(SPANISH), is(EXPECTED_DEFAULT_STAMP));
        assertThat(
                "A launch configuration with all languages support is not generating the expected "
                        + "processing stamp for German!",
                mStamp.getRelatedSearchesStamp(GERMAN), is(EXPECTED_DEFAULT_STAMP));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testUpdateUriForSelectedPosition() {
        setStandardDefaultLaunchConfiguration();
        Uri updatedUri = RelatedSearchesStamp.updateUriForSuggestionPosition(SAMPLE_URI, 3);
        String stampParam = updatedUri.getQueryParameter(RELATED_SEARCHES_STAMP_PARAM);
        assertThat("Appending the UI position of a chosen Related Searches suggestion doesn't have "
                        + "the expected prefix!",
                stampParam, startsWith(EXPECTED_DEFAULT_STAMP));
        assertThat("Appending the UI position of a chosen Related Searches suggestion doesn't have "
                        + "the expected psotion index!",
                stampParam, endsWith(EXPECTED_POSITION_ENDING + "3"));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesStamp"})
    public void testReplaceQueryParam() {
        Uri updatedQuery = RelatedSearchesStamp.replaceQueryParam(SAMPLE_URI, "q", "newQuery");
        assertTrue("Replacing a query parameter is not producing the expected CGI param!",
                updatedQuery.toString().contains("?q=newQuery"));
        String doubleUpdatedQuery =
                RelatedSearchesStamp.replaceQueryParam(updatedQuery, "q", "newerQuery").toString();
        assertTrue("Replacing a query parameter with a newer one is not producing the expected CGI "
                        + "param!",
                doubleUpdatedQuery.contains("?q=newerQuery"));
        assertFalse("Replacing a query parameter appears to fail to remove the original param!",
                doubleUpdatedQuery.contains("newQuery"));
        // Test removal
        String uriWithNoQ =
                RelatedSearchesStamp.replaceQueryParam(SAMPLE_URI, "q", null).toString();
        assertFalse("Removing a query parameter isn't working!", uriWithNoQ.contains("q"));
        // Test replacement of a param that doesn't exist
        String shouldBeUnchanged =
                RelatedSearchesStamp.replaceQueryParam(SAMPLE_URI, "qqq", "newQuery").toString();
        assertTrue("Replacing a non-existing parameter is removing an existing one!",
                shouldBeUnchanged.contains("?q=query"));
        assertFalse("Replacing a non-existing parameter is adding the new parameter anyway!",
                shouldBeUnchanged.contains("qqq"));
    }
}
