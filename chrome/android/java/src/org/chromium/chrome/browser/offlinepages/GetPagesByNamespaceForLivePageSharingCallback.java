// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.share.ShareParams;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/**
 * This callback will take the items in the Live Page Sharing namespace and check if there is a page
 * matching the url of the given tab. If there is such page, share the page; otherwise save the page
 * and share.
 */
public class GetPagesByNamespaceForLivePageSharingCallback
        implements Callback<List<OfflinePageItem>> {
    private Tab mTab;
    private Callback<ShareParams> mShareCallback;
    private OfflinePageBridge mBridge;

    public GetPagesByNamespaceForLivePageSharingCallback(
            Tab tab, final Callback<ShareParams> shareCallback, OfflinePageBridge bridge) {
        mTab = tab;
        mShareCallback = shareCallback;
        mBridge = bridge;
    }

    @Override
    public void onResult(List<OfflinePageItem> items) {
        // If there is already a page in the Live Page Sharing namespace and matches the url, share
        // it directly.
        for (OfflinePageItem item : items) {
            if (item.getUrl().equals(mTab.getUrl())) {
                OfflinePageUtils.sharePublishedPage(item, mTab.getWindowAndroid(), mShareCallback);
                return;
            }
        }
        // Otherwise, save the page within Live Page Sharing namespace and share it using content
        // URI.
        mBridge.savePage(mTab.getWebContents(),
                new ClientId(OfflinePageBridge.LIVE_PAGE_SHARING_NAMESPACE,
                        Integer.toString(mTab.getId())),
                new SavePageAndShareCallback(mTab.getWindowAndroid(), mShareCallback, mBridge));
    }
}