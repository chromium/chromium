// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.contextualsearch;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;

import java.net.MalformedURLException;
import java.net.URL;

/**
 * Builds a Search Request URL to be used to populate the content of the Bottom Sheet in response
 * to a particular Contextual Search.
 * The URL has a low-priority version to help with server overload, and helps manage the
 * fall-back to normal when that is needed after the low-priority version fails.
 * The URL building includes triggering of feature one-boxes on the SERP like a translation
 * One-box or a knowledge panel.
 */
class ContextualSearchRequest {
    private final boolean mWasPrefetch;
    private final Profile mProfile;

    private Uri mLowPriorityUri;
    private Uri mNormalPriorityUri;

    private boolean mIsLowPriority;
    private boolean mHasFailedLowPriorityLoad;
    private boolean mIsTranslationForced;
    private boolean mIsFullSearchUrlProvided;

    private static final String GWS_NORMAL_PRIORITY_SEARCH_PATH = "search";
    private static final String GWS_LOW_PRIORITY_SEARCH_PATH = "s";
    private static final String GWS_SEARCH_NO_SUGGESTIONS_PARAM = "sns";
    private static final String GWS_SEARCH_NO_SUGGESTIONS_PARAM_VALUE = "1";
    private static final String GWS_QUERY_PARAM = "q";
    private static final String CTXS_PARAM_PATTERN = "(ctxs=[^&]+)";
    private static final String CTXR_PARAM = "ctxr";
    private static final String PF_PARAM = "(\\&pf=\\w)";
    private static final String CTXS_TWO_REQUEST_PROTOCOL = "2";
    private static final String CTXSL_TRANS_PARAM = "ctxsl_trans";
    private static final String CTXSL_TRANS_PARAM_VALUE = "1";
    @VisibleForTesting static final String TLITE_SOURCE_LANGUAGE_PARAM = "tlitesl";
    private static final String TLITE_TARGET_LANGUAGE_PARAM = "tlitetl";
    private static final String TLITE_QUERY_PARAM = "tlitetxt";
    private static final String KP_TRIGGERING_MID_PARAM = "kgmid";

    /**
     * Creates a search request for the given search term without any alternate term and for
     * normal-priority loading capability only.
     *
     * @param profile The Profile associated with this request.
     * @param searchTerm The resolved search term.
     */
    ContextualSearchRequest(Profile profile, String searchTerm) {
        this(profile, searchTerm, false);
    }

    /**
     * Creates a search request for the given search term without any alternate term and for
     * low-priority loading capability if specified in the second parameter.
     *
     * @param profile The Profile associated with this request.
     * @param searchTerm The resolved search term.
     * @param isLowPriorityEnabled Whether the request can be made at a low priority.
     */
    ContextualSearchRequest(Profile profile, String searchTerm, boolean isLowPriorityEnabled) {
        this(profile, searchTerm, null, null, isLowPriorityEnabled, null, null);
    }

    /**
     * Creates a search request for the given URL without any alternate term or low priority loading
     * capability for preload.
     *
     * @param profile The Profile associated with this request.
     * @param searchUrlFull The URI for the full search to present in the overlay.
     */
    ContextualSearchRequest(Profile profile, @NonNull Uri searchUrlFull) {
        this(profile, null, null, null, false, searchUrlFull.toString(), null);
    }

    /**
     * Creates a search request for the given search term, unless the full search URL is provided in
     * the {@code searchUrlFull}. When the full URL is not provided the request also uses the given
     * alternate term, mid, and low-priority loading capability.
     *
     * <p>If the {@code searchUrlPreload} is provided then the {@code searchUrlFull} should also be
     * provided.
     *
     * @param profile The Profile associated with this request.
     * @param searchTerm The resolved search term.
     * @param alternateTerm The alternate search term.
     * @param mid The MID for an entity to use to trigger a Knowledge Panel, or an empty string. A
     *     MID is a unique identifier for an entity in the Search Knowledge Graph.
     * @param isLowPriorityEnabled Whether the request can be made at a low priority.
     * @param searchUrlFull The URL for the full search to present in the overlay, or empty.
     * @param searchUrlPreload The URL for the search to preload into the overlay, or empty.
     */
    ContextualSearchRequest(
            Profile profile,
            String searchTerm,
            @Nullable String alternateTerm,
            @Nullable String mid,
            boolean isLowPriorityEnabled,
            @Nullable String searchUrlFull,
            @Nullable String searchUrlPreload) {
        mProfile = profile;
        mWasPrefetch = isLowPriorityEnabled;
        mIsFullSearchUrlProvided = isGoogleUrl(searchUrlFull);
        mNormalPriorityUri =
                mIsFullSearchUrlProvided
                        ? Uri.parse(searchUrlFull)
                        : getUriTemplate(searchTerm, alternateTerm, mid, false);
        if (isLowPriorityEnabled) {
            if (isGoogleUrl(searchUrlPreload)) {
                mLowPriorityUri = Uri.parse(searchUrlPreload);
            } else {
                Uri baseLowPriorityUri = getUriTemplate(searchTerm, alternateTerm, mid, true);
                mLowPriorityUri = makeLowPriorityUri(baseLowPriorityUri);
            }
        } else {
            mLowPriorityUri = null;
        }
        mIsLowPriority = isLowPriorityEnabled;
    }

    /** Sets an indicator that the normal-priority URL should be used for this search request. */
    void setNormalPriority() {
        mIsLowPriority = false;
    }

    /**
     * @return whether the low priority URL is being used.
     */
    boolean isUsingLowPriority() {
        return mIsLowPriority;
    }

    /**
     * @return whether this request started as a prefetch request.
     */
    boolean wasPrefetch() {
        return mWasPrefetch;
    }

    /** Sets that this search request has failed. */
    void setHasFailed() {
        mHasFailedLowPriorityLoad = true;
    }

    /**
     * @return whether the load has failed for this search request or not.
     */
    boolean getHasFailed() {
        return mHasFailedLowPriorityLoad;
    }

    /**
     * Gets the search URL for this request.
     * @return either the low-priority or normal-priority URL for this search request.
     */
    String getSearchUrl() {
        return mIsLowPriority && mLowPriorityUri != null
                ? mLowPriorityUri.toString()
                : mNormalPriorityUri.toString();
    }

    /**
     * Returns whether the given URL is the current Contextual Search URL.
     * @param url The given URL.
     * @return Whether it is the current Contextual Search URL.
     */
    boolean isContextualSearchUrl(String url) {
        return url.equals(getSearchUrl());
    }

    /**
     * Returns the formatted Search URL, replacing the ctxs param with the ctxr param, so that
     * the SearchBox will becomes visible, while preserving the Answers Mode.
     *
     * @return The formatted Search URL.
     */
    String getSearchUrlForPromotion() {
        setNormalPriority();
        String searchUrl = getSearchUrl();

        URL url;
        try {
            url =
                    new URL(
                            searchUrl
                                    .replaceAll(CTXS_PARAM_PATTERN, CTXR_PARAM)
                                    .replaceAll(PF_PARAM, ""));
        } catch (MalformedURLException e) {
            url = null;
        }

        return url != null ? url.toString() : null;
    }

    /**
     * Adds translation parameters.
     * @param sourceLanguage The language of the original search term.
     * @param targetLanguage The language the that the user prefers.
     */
    void forceTranslation(String sourceLanguage, String targetLanguage) {
        mIsTranslationForced = true;
        // If the server is providing a full URL then we shouldn't alter it.
        if (mIsFullSearchUrlProvided || TextUtils.isEmpty(targetLanguage)) {
            return;
        }

        if (mLowPriorityUri != null) {
            mLowPriorityUri = makeTranslateUri(mLowPriorityUri, sourceLanguage, targetLanguage);
        }
        mNormalPriorityUri = makeTranslateUri(mNormalPriorityUri, sourceLanguage, targetLanguage);
    }

    /**
     * Adds translation parameters that will trigger auto-detection of the source language.
     * @param targetLanguage The language the that the user prefers.
     */
    void forceAutoDetectTranslation(String targetLanguage) {
        // Use the empty string for the source language in order to trigger auto-detect.
        forceTranslation("", targetLanguage);
    }

    /**
     * @return Whether translation was forced for this request (for testing only).
     */
    @VisibleForTesting
    boolean isTranslationForced() {
        return mIsTranslationForced;
    }

    /**
     * Uses TemplateUrlService to generate the url for the given query {@link String} for {@code
     * query} with the contextual search version param set.
     *
     * @param query The search term to use as the main query in the returned search url.
     * @param alternateTerm The alternate search term to use as an alternate suggestion.
     * @param mid The MID for an entity to use to trigger a Knowledge Panel, or an empty string. A
     *     MID is a unique identifier for an entity in the Search Knowledge Graph.
     * @param shouldPrefetch Whether the returned url should include a prefetch parameter.
     * @return A {@link Uri} that contains the url of the default search engine with {@code query}
     *     and {@code alternateTerm} inserted as parameters and contextual search and prefetch
     *     parameters conditionally set.
     */
    protected Uri getUriTemplate(
            String query,
            @Nullable String alternateTerm,
            @Nullable String mid,
            boolean shouldPrefetch) {
        // TODO(crbug.com/40549331): Avoid parsing the GURL as a Uri, and update
        // makeKPTriggeringUri to operate on GURLs.
        Uri uri =
                Uri.parse(
                        TemplateUrlServiceFactory.getForProfile(mProfile)
                                .getUrlForContextualSearchQuery(
                                        query,
                                        alternateTerm,
                                        shouldPrefetch,
                                        CTXS_TWO_REQUEST_PROTOCOL)
                                .getSpec());
        if (!TextUtils.isEmpty(mid)) uri = makeKPTriggeringUri(uri, mid);
        return uri;
    }

    /**
     * Judges if the given URL looks like a Google URL.
     * @param someUrl A URL to judge.
     * @return Whether it's pointing to Google infrastructure or not.
     */
    @VisibleForTesting
    boolean isGoogleUrl(@Nullable String someUrl) {
        return !TextUtils.isEmpty(someUrl) && UrlUtilitiesJni.get().isGoogleSubDomainUrl(someUrl);
    }

    /**
     * @return a low-priority {@code Uri} from the given base {@code Uri}.
     */
    private Uri makeLowPriorityUri(Uri baseUri) {
        if (baseUri.getPath() == null
                || !baseUri.getPath().contains(GWS_NORMAL_PRIORITY_SEARCH_PATH)) {
            return baseUri;
        }

        return baseUri.buildUpon()
                .path(GWS_LOW_PRIORITY_SEARCH_PATH)
                .appendQueryParameter(
                        GWS_SEARCH_NO_SUGGESTIONS_PARAM, GWS_SEARCH_NO_SUGGESTIONS_PARAM_VALUE)
                .build();
    }

    /**
     * Makes the given {@code Uri} into a similar Uri that triggers a Translate one-box.
     * @param baseUri The base Uri to build off of.
     * @param sourceLanguage The language of the original search term, or an empty string to
     *        auto-detect the source language.
     * @param targetLanguage The language that the user prefers, or an empty string to
     *        use server-side heuristics for the target language.
     * @return A {@link Uri} that has additional parameters for Translate appropriately set.
     */
    private Uri makeTranslateUri(Uri baseUri, String sourceLanguage, String targetLanguage) {
        Uri.Builder builder = baseUri.buildUpon();
        builder.appendQueryParameter(CTXSL_TRANS_PARAM, CTXSL_TRANS_PARAM_VALUE);
        if (!sourceLanguage.isEmpty()) {
            builder.appendQueryParameter(TLITE_SOURCE_LANGUAGE_PARAM, sourceLanguage);
        }
        if (!targetLanguage.isEmpty()) {
            builder.appendQueryParameter(TLITE_TARGET_LANGUAGE_PARAM, targetLanguage);
        }
        builder.appendQueryParameter(TLITE_QUERY_PARAM, baseUri.getQueryParameter(GWS_QUERY_PARAM));
        return builder.build();
    }

    /**
     * Converts a URI to a URI that will trigger a Knowledge Panel for the given entity.
     * @param baseUri The base URI to convert.
     * @param mid The un-encoded MID (unique identifier) for an entity to use to trigger a Knowledge
     *            Panel.
     * @return The converted URI.
     */
    private Uri makeKPTriggeringUri(Uri baseUri, String mid) {
        // Need to add a param like &kgmid=/m/0cqt90
        // Note that the mid must not be escaped - appendQueryParameter will take care of that.
        return baseUri.buildUpon().appendQueryParameter(KP_TRIGGERING_MID_PARAM, mid).build();
    }
}
