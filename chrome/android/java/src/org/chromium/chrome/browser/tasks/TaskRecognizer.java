// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.net.Uri;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabPanel;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchContext;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm.CardTag;
import org.chromium.chrome.browser.contextualsearch.SimpleSearchTermResolver;
import org.chromium.chrome.browser.contextualsearch.SimpleSearchTermResolver.ResolveResponse;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/**
 * Recognizes that a Tab is associated with a product.
 * Uses requests through the SimpleSearchTermResolver to make a Contextual Search request to
 * get a card for a section of text.  If that card is a Product, then we may take action.
 */
public class TaskRecognizer extends EmptyTabObserver implements ResolveResponse {
    // TODO(yusufo): Consider converting this to use HintlessActivityTabObserver.
    private static final String TAG = "TaskRecognizer";

    // TODO(yusufo): remove when the server starts returning CardTags.
    private static final String UNICODE_STAR = "\u2605";

    /** Our singleton instance. */
    private static TaskRecognizer sInstance;

    /** The Tab that is in use. */
    private Tab mTabInUse;

    /** A Context that is in use, or null. */
    private ContextualSearchContext mContext;

    /** Gets the singleton instance for this class. */
    public static TaskRecognizer getInstance(Tab tab) {
        if (sInstance == null) sInstance = new TaskRecognizer(tab);
        return sInstance;
    }

    /**
     * Creates a {@link TaskRecognizer} for the given tab.
     * @param tab The tab to work with.
     */
    public static void createForTab(Tab tab) {
        getInstance(tab);
    }

    /**
     * Constructs a Task Recognizer for the given Tab.
     * @param tab The {@link Tab} to track with this helper.
     */
    private TaskRecognizer(Tab tab) {
        tab.addObserver(this);
    }

    /**
     * Try to recognize that the given {@link Tab} is about a product and show some product info.
     * @param tab The tab that might be about a product.  Must be the current front tab.
     */
    private void tryToShowProduct(Tab tab) {
        boolean isCurrentSelectedTab = tab != null && tab.getActivity() != null
                && tab.equals(tab.getActivity().getActivityTab());
        if (mTabInUse != null || !isCurrentSelectedTab) {
            return;
        }

        // TODO(yusufo): filter based on other criteria, e.g. Incognito.
        boolean inProgress = resolveTitleAndShowOverlay(tab, this);
        if (inProgress) {
            Log.v(TAG, "Trying to show product info for tab: " + tab);
            mTabInUse = tab;
        }
    }

    /**
     * Tries to identify a product in the given tab, and calls the given callback with the results.
     * @param tab The tab that we're trying to identify a product in.  We currently use the tab's
     *        title as a hack for what to use for identification purposes.
     * @param responseCallback The callback to call.
     * @return Whether we were able to issue a request.
     */
    private boolean resolveTitleAndShowOverlay(Tab tab, ResolveResponse responseCallback) {
        if (mTabInUse != null) return false;

        mTabInUse = tab;
        String pageTitle = tab.getTitle();
        String pageUrl = tab.getUrl();
        WebContents webContents = tab.getWebContents();
        if (TextUtils.isEmpty(pageTitle) || TextUtils.isEmpty(pageUrl) || webContents == null) {
            Log.w(TAG, "not a good page. :-(");
            return false;
        }

        if (mContext != null) mContext.destroy();
        int insertionPointLocation = pageTitle.length() / 2;
        mContext = ContextualSearchContext.getContextForInsertionPoint(
                pageTitle, insertionPointLocation);
        if (mContext == null) {
            Log.i(TAG, "not a context. :-(");
            return false;
        }

        Log.v(TAG, "startSearchTermResolutionRequest");
        SimpleSearchTermResolver.getInstance().startSearchTermResolutionRequest(
                webContents, mContext, responseCallback);
        return true;
    }

    /** @return Whether the given {@link ResolvedSearchTerm} identified a product. */
    private boolean looksLikeAProduct(ResolvedSearchTerm resolvedSearchTerm) {
        Log.v(TAG,
                "looksLikeAProduct: " + (resolvedSearchTerm.cardTagEnum() == CardTag.CT_PRODUCT));
        if (resolvedSearchTerm.cardTagEnum() == CardTag.CT_PRODUCT) return true;

        // Fallback onto our "has-a-star" hack.
        return (resolvedSearchTerm.cardTagEnum() == CardTag.CT_NONE
                && resolvedSearchTerm.caption().contains(UNICODE_STAR));
    }

    /**
     * Creates an {@code EphemeralTab} for the given searchUrl using details from the given
     * {@code ResolvedSearchterm}.
     */
    private void createEphemeralTabFor(
            Tab activeTab, ResolvedSearchTerm resolvedSearchTerm, Uri searchUrl) {
        if (activeTab == null || activeTab.getActivity() == null) return;

        EphemeralTabPanel displayPanel = activeTab.getActivity().getEphemeralTabPanel();
        if (displayPanel != null) {
            displayPanel.requestOpenPanel(searchUrl.toString(), resolvedSearchTerm.displayText(),
                    activeTab.isIncognito());
        }
    }

    /** ResolveResponse overrides. */
    @Override
    public void onResolveResponse(ResolvedSearchTerm resolvedSearchTerm, Uri searchUri) {
        Tab activeTab = mTabInUse;
        mTabInUse = null;
        if (looksLikeAProduct(resolvedSearchTerm) && activeTab != null) {
            createEphemeralTabFor(activeTab, resolvedSearchTerm, searchUri);
        }
    }

    /** EmptyTabObserver overrides. */
    @Override
    public void onPageLoadFinished(Tab tab, String url) {
        tryToShowProduct(tab);
    }

    @Override
    public void onTitleUpdated(Tab tab) {
        tryToShowProduct(tab);
    }
}
