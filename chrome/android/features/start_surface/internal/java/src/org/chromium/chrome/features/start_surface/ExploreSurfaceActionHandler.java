// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import androidx.annotation.NonNull;

import com.google.android.libraries.feed.api.client.knowncontent.ContentMetadata;

import org.chromium.chrome.browser.feed.FeedLoggingBridge;
import org.chromium.chrome.browser.feed.FeedOfflineIndicator;
import org.chromium.chrome.browser.feed.action.FeedActionHandler;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;

/** Implementation of the {@link ActionApi} for the explore surface. */
class ExploreSurfaceActionHandler extends FeedActionHandler {
    ExploreSurfaceActionHandler(@NonNull NativePageNavigationDelegate delegate,
            @NonNull Runnable suggestionConsumedObserver,
            @NonNull FeedOfflineIndicator offlineIndicator,
            @NonNull OfflinePageBridge offlinePageBridge,
            @NonNull FeedLoggingBridge loggingBridge) {
        super(delegate, suggestionConsumedObserver, offlineIndicator, offlinePageBridge,
                loggingBridge);
    }

    // TODO(crbug.com/982018): Support download and lean more actions.

    @Override
    public void downloadUrl(ContentMetadata contentMetadata) {}

    @Override
    public boolean canDownloadUrl() {
        return false;
    }

    @Override
    public void learnMore() {}

    @Override
    public boolean canLearnMore() {
        return false;
    }
}