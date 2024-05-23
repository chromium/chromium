// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;

/** UrlImageProvider.UrlImageSource implementation for production. */
public class UrlImageSourceImpl implements UrlImageProvider.UrlImageSource {
    private final Context mContext;
    private final Profile mProfile;
    private final TabContentManager mTabContentManager;

    UrlImageSourceImpl(Context context, Profile profile, TabContentManager tabContentManager) {
        mContext = context;
        mProfile = profile;
        mTabContentManager = tabContentManager;
    }

    /** Implements {@link UrlImageProvider.UrlImageSource} */
    @Override
    public LargeIconBridge createLargeIconBridge() {
        return new LargeIconBridge(mProfile);
    }

    @Override
    public ThumbnailProvider createThumbnailProvider() {
        return (tabId, thumbnailSize, callback, forceUpdate, writeBack, isSelected) -> {
            mTabContentManager.getTabThumbnailWithCallback(
                    tabId, thumbnailSize, callback, forceUpdate, writeBack);
        };
    }

    @Override
    public RoundedIconGenerator createIconGenerator() {
        return FaviconUtils.createRoundedRectangleIconGenerator(mContext);
    }
}
