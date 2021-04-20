// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.NtpListContentManager.FeedContent;
import org.chromium.chrome.browser.feed.NtpListContentManager.FeedContentMetadata;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.v2.FeedStream.InteractionsListener;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * A helper class to get the Discover articles metadata and set as continuous navigation data
 * source.
 */
public class ContinuousFeedNavigationHelper
        implements ContentChangedListener, InteractionsListener {
    private final TabModelSelector mTabModelSelector;
    private ContinuousNavigationMetadata mContinuousNavMetadata;

    public ContinuousFeedNavigationHelper(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    @Override
    public void onContentChanged(@Nullable List<FeedContent> contents) {
        if (contents == null) return;

        List<PageItem> pageItems = new ArrayList<>();
        for (FeedContent content : contents) {
            FeedContentMetadata metadata = content.getMetadata();
            if (metadata == null) continue;

            GURL url = metadata.getUrl();
            String title = metadata.getTitle();
            if (!GURL.isEmptyOrInvalid(url) && title != null && !title.isEmpty()) {
                pageItems.add(new PageItem(url, title));
            }
        }
        if (pageItems.size() == 0) {
            mContinuousNavMetadata = null;
            return;
        }

        List<PageGroup> pageGroups = new ArrayList<>();
        // TODO(crbug.com/1200802): Add i18n
        pageGroups.add(new PageGroup("Discover", false, pageItems));
        mContinuousNavMetadata = new ContinuousNavigationMetadata(
                /*url=*/null, /*query=*/null, PageCategory.DISCOVER, pageGroups);
    }

    @Override
    public void onNavigate(String url) {
        if (mContinuousNavMetadata == null) return;

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CONTINUOUS_SEARCH)
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.CONTINUOUS_FEEDS)) {
            return;
        }

        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab != null) {
            ContinuousNavigationUserData userData = ContinuousNavigationUserData.getForTab(tab);
            if (userData != null) {
                GURL gurl = new GURL(url);
                assert !GURL.isEmptyOrInvalid(gurl);
                userData.updateData(mContinuousNavMetadata, gurl);
            }
        }
    }
}
