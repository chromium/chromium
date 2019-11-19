// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.net.Uri;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm.CardTag;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/**
 * Provides
 */
public class SimpleSearchTermResolver {
    /**
     * Provides a callback that will be called when the server responds with the Resolved
     * Search Term, or an error.
     */
    public interface ResolveResponse {
        void onResolveResponse(ResolvedSearchTerm resolvedSearchTerm, Uri searchUri);
    }

    private static final String TAG = "TTS Resolver";

    // Our singleton instance.
    private static SimpleSearchTermResolver sInstance;

    // Pointer to the native instance of this class.
    private long mNativePointer;
    private Tab mTabInUse;
    private ResolveResponse mResponseCallback;

    private ContextualSearchContext mContext;

    /** Gets the singleton instance for this class. */
    public static SimpleSearchTermResolver getInstance() {
        if (sInstance == null) sInstance = new SimpleSearchTermResolver();
        return sInstance;
    }

    /**
     * Starts a Search Term Resolution request for the given {@link ContextualSearchContext}.
     * @param baseWebContents Provides some context info, like the URL of the page.
     * @param contextualSearchContext Provides most of the Context info, including selection
     *        location.
     * @param responseCallback A {@link ResolveResponse} instance, so we can call
     *        {@link ResolveResponse#onResolveResponse} when the response comes in.
     */
    public void startSearchTermResolutionRequest(WebContents baseWebContents,
            ContextualSearchContext contextualSearchContext, ResolveResponse responseCallback) {
        assert mResponseCallback == null;
        mResponseCallback = responseCallback;
        if (baseWebContents != null && contextualSearchContext != null
                && contextualSearchContext.canResolve()) {
            Log.i(TAG,
                    "calling SimpleSearchTermResolverJni.get().startSearchTermResolutionRequest.");
            SimpleSearchTermResolverJni.get().startSearchTermResolutionRequest(mNativePointer,
                    SimpleSearchTermResolver.this, contextualSearchContext, baseWebContents);
        }
    }

    /**
     * Called in response to the
     * {@link
     * ContextualSearchManager#SimpleSearchTermResolverJni.get().startSearchTermResolutionRequest}
     * method. If {@code SimpleSearchTermResolverJni.get().startSearchTermResolutionRequest} is
     * called with a previous request sill pending our native delegate is supposed to cancel all
     * previous requests.  So this code should only be called with data corresponding to the most
     * recent request.
     * @param isNetworkUnavailable Indicates if the network is unavailable, in which case all other
     *        parameters should be ignored.
     * @param responseCode The HTTP response code. If the code is not OK, the query should be
     *        ignored.
     * @param searchTerm The term to use in our subsequent search.
     * @param displayText The text to display in our UX.
     * @param alternateTerm The alternate term to display on the results page.
     * @param mid the MID for an entity to use to trigger a Knowledge Panel, or an empty string.
     *        A MID is a unique identifier for an entity in the Search Knowledge Graph.
     * @param doPreventPreload Whether we should prevent preloading on this search.
     * @param selectionStartAdjust A positive number of characters that the start of the existing
     *        selection should be expanded by.
     * @param selectionEndAdjust A positive number of characters that the end of the existing
     *        selection should be expanded by.
     * @param contextLanguage The language of the original search term, or an empty string.
     * @param thumbnailUrl The URL of the thumbnail to display in our UX.
     * @param caption The caption to display.
     * @param quickActionUri The URI for the intent associated with the quick action.
     * @param quickActionCategory The {@link QuickActionCategory} for the quick action.
     * @param loggedEventId The EventID logged by the server, which should be recorded and sent back
     *        to the server along with user action results in a subsequent request.
     * @param searchUrlFull The URL for the full search to present in the overlay, or empty.
     * @param searchUrlPreload The URL for the search to preload into the overlay, or empty.
     * @param cocaCardTag The primary internal Coca card tag for the response, or {@code 0} if none.
     */
    @CalledByNative
    public void onSearchTermResolutionResponse(boolean isNetworkUnavailable, int responseCode,
            final String searchTerm, final String displayText, final String alternateTerm,
            final String mid, boolean doPreventPreload, int selectionStartAdjust,
            int selectionEndAdjust, final String contextLanguage, final String thumbnailUrl,
            final String caption, final String quickActionUri,
            @QuickActionCategory final int quickActionCategory, final long loggedEventId,
            final String searchUrlFull, final String searchUrlPreload,
            @CardTag final int cocaCardTag) {
        ResolvedSearchTerm resolvedSearchTerm = new ResolvedSearchTerm(isNetworkUnavailable,
                responseCode, searchTerm, displayText, alternateTerm, mid, doPreventPreload,
                selectionStartAdjust, selectionEndAdjust, contextLanguage, thumbnailUrl, caption,
                quickActionUri, quickActionCategory, loggedEventId, searchUrlFull, searchUrlPreload,
                cocaCardTag);
        Log.v(TAG, "onSearchTermResolutionResponse received with " + resolvedSearchTerm);
        if (!TextUtils.isEmpty(resolvedSearchTerm.searchTerm())) {
            ResolveResponse responseCallback = mResponseCallback;
            mResponseCallback = null;
            assert responseCallback != null;
            Uri searchUrl = Uri.parse(makeSearchUrl(resolvedSearchTerm));
            responseCallback.onResolveResponse(resolvedSearchTerm, searchUrl);
        }
        mTabInUse = null;
    }

    /** Constructs the singleton instance. */
    private SimpleSearchTermResolver() {
        mNativePointer = SimpleSearchTermResolverJni.get().init(SimpleSearchTermResolver.this);
    }

    /** Makes a Search URL from the given {@link ResolvedSearchTerm}. */
    private String makeSearchUrl(ResolvedSearchTerm resolvedSearchTerm) {
        return new ContextualSearchRequest(resolvedSearchTerm.searchTerm()).getSearchUrl();
    }

    /**
     * This method should be called to clean up storage when an instance of this class is
     * no longer in use.  The SimpleSearchTermResolverJni.get().destroy will call the destructor on
     * the native instance.
     */
    void destroy() {
        assert mNativePointer != 0;
        SimpleSearchTermResolverJni.get().destroy(mNativePointer, SimpleSearchTermResolver.this);
        mNativePointer = 0;
    }

    @NativeMethods
    interface Natives {
        long init(SimpleSearchTermResolver caller);
        void destroy(long nativeSimpleSearchTermResolver, SimpleSearchTermResolver caller);
        void startSearchTermResolutionRequest(long nativeSimpleSearchTermResolver,
                SimpleSearchTermResolver caller, ContextualSearchContext contextualSearchContext,
                WebContents baseWebContents);
    }
}
