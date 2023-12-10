// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.base.WindowAndroid;

/** Helper class that wraps accessibility state information for {Tab}. */
public class AccessibilityTabHelper extends EmptyTabObserver implements UserData {
    public static final Class<AccessibilityTabHelper> USER_DATA_KEY = AccessibilityTabHelper.class;

    private final Tab mTab;
    private WebContents mWebContents;

    /**
     * Creates an instance of {AccessibilityTabHelper} for the given tab.
     * @param tab Tab to observe.
     */
    public static void createForTab(Tab tab) {
        tab.getUserDataHost().setUserData(USER_DATA_KEY, new AccessibilityTabHelper(tab));
    }

    /**
     * Constructs a new {AccessibilityTabHelper} object for the given tab.
     * @param tab Tab to observe.
     */
    private AccessibilityTabHelper(Tab tab) {
        mTab = tab;
        mTab.addObserver(this);
        mWebContents = tab.getWebContents();
        if (mWebContents != null) updateWebContentsAccessibilityStateForTab(tab);
    }

    /**
     * Communicate accessibility state information for this embedder to the
     * {WebContentsAccessibility} object for the given tab. These values are used to determine
     * which accessibility features are enabled for the associated {WebContents}; which is dependent
     * on both user device settings, and the embedder of this Tab.
     * @param tab Tab to update state for.
     */
    private void updateWebContentsAccessibilityStateForTab(Tab tab) {
        assert tab.getWebContents() != null;
        WebContentsAccessibility wcax =
                WebContentsAccessibility.fromWebContents(tab.getWebContents());

        // For browser tabs, we want to set accessibility focus to the page when it loads. This
        // is not the default behavior for embedded web views.
        wcax.setShouldFocusOnPageLoad(true);

        // Enable image descriptions feature normally, but not for Chrome Custom Tabs.
        wcax.setIsImageDescriptionsCandidate(!tab.isCustomTab());

        // Enable Auto-disable Accessibility feature normally, but not for Chrome Custom Tabs.
        wcax.setIsAutoDisableAccessibilityCandidate(!tab.isCustomTab());
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, WindowAndroid window) {
        // When the activity attachment changes, we communicate the current accessibility state
        // since the attachment change could be part of switching embedders (e.g. CCT to Chrome).
        if (tab.getWebContents() != null) updateWebContentsAccessibilityStateForTab(tab);
    }

    @Override
    public void onContentChanged(Tab tab) {
        // When the content changes, if the current |mWebContents| is different than the given Tab's
        // we communicate state for the new web contents (e.g. web contents was changed at runtime).
        if (mWebContents == tab.getWebContents()) return;

        if (tab.getWebContents() != null) updateWebContentsAccessibilityStateForTab(tab);
        mWebContents = tab.getWebContents();
    }

    @Override
    public void destroy() {
        mTab.removeObserver(this);
    }
}
