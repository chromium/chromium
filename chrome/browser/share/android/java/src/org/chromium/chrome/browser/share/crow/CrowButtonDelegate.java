// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.crow;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/**
 * Interface to expose the experimental 'Crow' sharing feature in the App menu
 * footer and Feed to external classes.
 */
public interface CrowButtonDelegate {
    /**
     * @return Whether to show the chip for |url|.
     *
     * @param url URL for current tab where the app menu was launched.
     * @param callback Callback informing whether the feature is enabled for 'url.'
     */
    void isEnabledForSite(GURL url, Callback<Boolean> callback);

    /**
     * Launches a custom tab to a server-provided interaction flow.
     * Uses URL defined by the study config.
     *
     * @param currentContext the current Context for which the user activated an
     *                        entry point.
     * @param pageUrl URL for the page; passed in rather than derived from currentTab
     *     or WebContents's lastCommittedURL as it was used to construct UI in the caller.
     * @param canonicalPageUrl Canonical URL for 'pageUrl.' May be empty.
     * @param isFollowing Whether the user is following the associated host in the feed.
     */
    void launchCustomTab(Tab tab, Context currentContext, GURL pageUrl, GURL canonicalPageUrl,
            boolean isFollowing);

    /**
     * @return experiment-configured chip text.
     */
    String getButtonText();

    /**
     * Obtains the Canonical URL for a Tab.
     * @param tab The tab for which to find the canonical URL.
     * @param Callback<String> callback returning the canonical URL, or empty.
     */
    void requestCanonicalUrl(Tab tab, Callback<GURL> url);

    /**
     * Returns a URL that can be loaded for the web-hosted piece of this feature.
     *
     * @param pageUrl the URL of the page with content.
     * @param canonicalPageUrl canonical URL for |pageUrl|, may be an empty GURL.
     * @param isFollowing whether the user is following |pageUrl|'s site on the feed.
     */
    String getUrlForWebFlow(GURL pageUrl, GURL canonicalPageUrl, boolean isFollowing);
}
