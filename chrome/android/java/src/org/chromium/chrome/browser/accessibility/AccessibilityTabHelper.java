// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabWebContentsUserData;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.base.WindowAndroid;

/** Helper class that wraps accessibility state information for {@link Tab}. */
@NullMarked
public class AccessibilityTabHelper extends TabWebContentsUserData {
    public static final Class<AccessibilityTabHelper> USER_DATA_KEY = AccessibilityTabHelper.class;

    private final Tab mTab;

    /**
     * Retrieves the {@link AccessibilityTabHelper} for the given {@link Tab}, creating it if it
     * doesn't already exist.
     *
     * @param tab The Tab to get the helper for.
     * @return The {@link AccessibilityTabHelper}, or null if WebContents is null and no helper has
     *     been created yet.
     */
    public static @Nullable AccessibilityTabHelper from(Tab tab) {
        assert !tab.isDestroyed();
        AccessibilityTabHelper helper = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (helper == null) {
            if (tab.getWebContents() == null) return null;
            helper = attachToTab(USER_DATA_KEY, new AccessibilityTabHelper(tab));
        }
        return helper;
    }

    /**
     * Constructs a new {@link AccessibilityTabHelper} object for the given Tab.
     *
     * @param tab Tab to observe.
     */
    private AccessibilityTabHelper(Tab tab) {
        super(tab);
        mTab = tab;
    }

    @Override
    public void initWebContents(WebContents webContents) {
        updateWebContentsAccessibilityState(webContents);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {}

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        if (window == null) return;
        // When the activity attachment changes, we communicate the current accessibility state
        // since the attachment change could be part of switching embedders (e.g. CCT to Chrome).
        WebContents webContents = getWebContents();
        if (webContents != null) {
            updateWebContentsAccessibilityState(webContents);
        }
    }

    /**
     * Communicate accessibility state information for this embedder to the {@link
     * WebContentsAccessibility} object for the given tab. These values are used to determine which
     * accessibility features are enabled for the associated {@link WebContents}; which is dependent
     * on both user device settings, and the embedder of this Tab.
     */
    private void updateWebContentsAccessibilityState(WebContents webContents) {
        WebContentsAccessibility wcax = WebContentsAccessibility.fromWebContents(webContents);
        assumeNonNull(wcax);

        boolean isCustomTab = mTab.isCustomTab();

        // For browser tabs, we want to set accessibility focus to the page when it loads. This
        // is not the default behavior for embedded web views.
        wcax.setShouldFocusOnPageLoad(true);

        // Enable image descriptions feature normally, but not for Chrome Custom Tabs.
        wcax.setIsImageDescriptionsCandidate(!isCustomTab);

        // Enable Auto-disable Accessibility feature normally, but not for Chrome Custom Tabs.
        wcax.setIsAutoDisableAccessibilityCandidate(!isCustomTab);
    }
}
