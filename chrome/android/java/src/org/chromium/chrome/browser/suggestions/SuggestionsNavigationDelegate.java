// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.app.Activity;

import org.chromium.chrome.browser.native_page.NativePageNavigationDelegateImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/**
 * Extension of {@link NativePageNavigationDelegate} with suggestions-specific methods.
 */
public class SuggestionsNavigationDelegate extends NativePageNavigationDelegateImpl {
    public SuggestionsNavigationDelegate(Activity activity, Profile profile, NativePageHost host,
            TabModelSelector tabModelSelector, Tab tab) {
        super(activity, profile, host, tabModelSelector, tab);
    }

    /**
     * Opens the suggestions page without recording metrics.
     *
     * @param windowOpenDisposition How to open (new window, current tab, etc).
     * @param url The url to navigate to.
     * @param inGroup Whether the navigation is in a group.
     */
    public void navigateToSuggestionUrl(int windowOpenDisposition, String url, boolean inGroup) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK);
        if (inGroup) {
            openUrlInGroup(windowOpenDisposition, loadUrlParams);
        } else {
            openUrl(windowOpenDisposition, loadUrlParams);
        }
    }
}
