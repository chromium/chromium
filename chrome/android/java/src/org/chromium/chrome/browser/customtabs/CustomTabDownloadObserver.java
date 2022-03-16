// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.download.new_download_tab.NewDownloadTab;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;

import javax.inject.Inject;

/**
 * A {@link TabObserver} that determines whether a custom tab navigation should show the new
 * download UI.
 */
public class CustomTabDownloadObserver extends EmptyTabObserver {
    private final TabObserverRegistrar mTabObserverRegistrar;

    @Inject
    public CustomTabDownloadObserver(TabObserverRegistrar tabObserverRegistrar) {
        mTabObserverRegistrar = tabObserverRegistrar;
        mTabObserverRegistrar.registerTabObserver(this);
    }

    @Override
    public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
        // For a navigation from page A to page B, there can be any number of redirects in between.
        // The first navigation which opens the custom tab will have a transition of type FROM_API.
        // Each redirect can then be treated as its own navigation with a separate call to this
        // method. This creates a mask which keeps this observer alive during the first chain of
        // navigations only. After that, this observer is unregistered.
        if ((navigation.pageTransition()
                    & (PageTransition.FROM_API | PageTransition.SERVER_REDIRECT
                            | PageTransition.CLIENT_REDIRECT))
                == 0) {
            unregister();
            return;
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_NEW_DOWNLOAD_TAB)
                && navigation.isDownload()) {
            NewDownloadTab.from(tab).show();
        }
    }

    private void unregister() {
        mTabObserverRegistrar.unregisterTabObserver(this);
    }
}
