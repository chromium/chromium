// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Handles the management of the Related Searches processing "stamp" CGI parameter.
 *
 * <p>The design is for the parameter to have an additional section appended to it during each phase
 * of processing, starting with the client request, adding backend response status, returning that
 * to the client, and if one of the suggested Related Searches queries is chosen then user-choice
 * information from the client will be sent to the server again (and logged).
 *
 * <p>The initial client request records the recipe used to create the request (and associated
 * experiments). It also includes whether the client wants this request to be handled just like a
 * normal Contextual Search request in order to do a "dark launch" that processes without changes
 * detectable by the end user.
 *
 * <p>The schema for the stamp is documented in go/rsearches-dd. There's a snapshot of the schema
 * below.
 */
class RelatedSearchesStamp {
    // Related Searches "stamp" building and accessing details.
    static final String STAMP_PARAMETER = "ctxsl_rs";
    private static final String RELATED_SEARCHES_STAMP_VERSION = "1";
    private static final String RELATED_SEARCHES_EXPERIMENT_RECIPE_STAGE = "R";
    private static final String RELATED_SEARCHES_NO_EXPERIMENT = "n";
    private static final String RELATED_SEARCHES_LANGUAGE_RESTRICTION = "l";
    private static final String RELATED_SEARCHES_USER_INTERACTION = "U";
    private static final String RELATED_SEARCHES_SELECTED_POSITION = "p";
    private static final String NO_EXPERIMENT_STAMP =
            RELATED_SEARCHES_STAMP_VERSION
                    + RELATED_SEARCHES_EXPERIMENT_RECIPE_STAGE
                    + RELATED_SEARCHES_NO_EXPERIMENT;

    private final ContextualSearchPolicy mPolicy;

    /**
     * Creates a Related Searches Stamp handling instance that works with the given {@code
     * ContextualSearchPolicy}
     */
    RelatedSearchesStamp(ContextualSearchPolicy policy) {
        mPolicy = policy;
    }

    /**
     * Replaces the given query parameter in the given {@code Uri}.
     * @param baseUri The Uri to modify.
     * @param paramName The name of the CGI param to alter.
     * @param paramValue The value to set on the given CGI parameter, or {@code null} if the CGI
     *         parameter should be removed instead of replaced.
     * TODO(donnd): move this into some kind of utility class.
     */
    public static Uri replaceQueryParam(
            Uri baseUri, String paramName, @Nullable String paramValue) {
        final Uri.Builder newUri = baseUri.buildUpon().clearQuery();
        for (String param : baseUri.getQueryParameterNames()) {
            String value = baseUri.getQueryParameter(param);
            if (param.equals(paramName)) value = paramValue;
            if (value != null) newUri.appendQueryParameter(param, value);
        }
        return newUri.build();
    }

    /*
     Here's a snapshot of the schema for Version 1 to use as a quick reference.
     The documentation at go/rsearches-dd is the authoritative reference:
     Stages and codes (as of 1/26/21):
     R - Recipe for this experiment
       u - url-only
       c - content-only
       b - both url & content
       n - none (the user flipped the flag)
       Optional digit "l" -- there was a language restriction.
       Final letter to indicate verbosity:
         "d" to indicate a dark launch (client wants to behave like the experiment is not being
             activated so the server should send back regular TTS results).
         "v" for verbose
         "x" for extreme
     C - Claire response
       u - url-based
       c - content-based
     U - User interaction // TODO(donnd): add support when sent by the client.
       p# - position number, e.g. p0 for "user clicked on position 0".
       TBD additional user interaction, e.g. # of seconds viewing the SERP.
    */

    /**
     * Gets the runtime processing stamp for Related Searches. This typically gets the value from a
     * param from a Field Trial Feature.
     *
     * @param basePageLanguage The language of the page, to check for server support.
     * @return A {@code String} whose value describes the schema version and current processing of
     *     Related Searches, or an empty string if the user is not qualified to request Related
     *     Searches or the feature is not enabled.
     */
    String getRelatedSearchesStamp(String basePageLanguage) {
        if (!isQualifiedForRelatedSearches(basePageLanguage)
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.RELATED_SEARCHES_SWITCH)) {
            return "";
        }

        boolean isLanguageRestricted = !TextUtils.isEmpty(getAllowedLanguages());
        return buildRelatedSearchesStamp(isLanguageRestricted);
    }

    /**
     * Determines if the current user is qualified for Related Searches. There may be language
     * and privacy restrictions on whether users can activate Related Searches, and some of these
     * requirements are determined at runtime based on Variations params.
     * @param basePageLanguage The language of the page, to check for server support.
     * @return Whether the user could do a Related Searches request if Feature-enabled.
     */
    boolean isQualifiedForRelatedSearches(String basePageLanguage) {
        return isLanguageQualified(basePageLanguage)
                && mPolicy.hasSendUrlPermissions()
                && mPolicy.isContextualSearchFullyEnabled();
    }

    /**
     * Updates the given URI to indicate that the user selected a suggestion at the given index.
     * @param searchUri A URI from the server for a Related Searches suggestion.
     * @param suggestionIndex The index
     * @return The supplied URI with an updated "stamp" parameter that now indicates that the user
     *     selected that suggestion when it was in the position specified by the index.
     */
    static Uri updateUriForSuggestionPosition(Uri searchUri, int suggestionIndex) {
        String currentStamp = searchUri.getQueryParameter(STAMP_PARAMETER);
        if (currentStamp == null || currentStamp.isEmpty()) return searchUri;

        String chosenPositionCode =
                RELATED_SEARCHES_USER_INTERACTION
                        + RELATED_SEARCHES_SELECTED_POSITION
                        + Integer.toString(suggestionIndex);
        return replaceQueryParam(searchUri, STAMP_PARAMETER, currentStamp + chosenPositionCode);
    }

    /**
     * Checks if the language of the page qualifies for Related Searches.
     * We check the Variations config for a parameter that lists allowed languages so we can know
     * what the server currently supports. If there's no allow list then any language will work.
     * @param basePageLanguage The language of the page, to check for server support.
     * @return whether the supplied parameter satisfies the current language requirement.
     */
    private boolean isLanguageQualified(String basePageLanguage) {
        String allowedLanguages = getAllowedLanguages();
        return TextUtils.isEmpty(allowedLanguages) || allowedLanguages.contains(basePageLanguage);
    }

    /**
     * Builds the "stamp" that tracks the processing of Related Searches and describes what was
     * done at each stage using a shorthand notation. The notation is described in go/rsearches-dd
     * here: http://doc/1DryD8NAP5LQAo326LnxbqkIDCNfiCOB7ak3gAYaNWAM#bookmark=id.nx7ivu2upqw
     * <p>The first stage is built here: "1" for schema version one, "R" for the configuration
     * Recipe which has a character describing how we'll formulate the search. Typically all of
     * this comes from the Variations config at runtime. We programmatically append an "l" that
     * indicates a language restriction (when present).
     * @param isLanguageRestricted Whether there are any language restrictions needed by the
     *        server.
     * @return A string that represents and encoded description of the current request processing.
     */
    private String buildRelatedSearchesStamp(boolean isLanguageRestricted) {
        String experimentConfigStamp =
                ContextualSearchFieldTrial.getRelatedSearchesExperimentConfigurationStamp();
        String ret = experimentConfigStamp;
        if (isLanguageRestricted) {
            ret += RELATED_SEARCHES_LANGUAGE_RESTRICTION;
        }

        return ret;
    }

    /**
     * get the allowed languages for the related searches.
     * @return A string that contains the allowed languages, or empty string which means all the
     *         languages are allowed.
     */
    private String getAllowedLanguages() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.RELATED_SEARCHES_ALL_LANGUAGE)) {
            return "";
        }

        return ContextualSearchFieldTrial.RELATED_SEARCHES_LANGUAGE_DEFAULT_ALLOWLIST;
    }
}
