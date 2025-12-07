// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.NetError;
import org.chromium.ui.base.WindowAndroid;

/** Monitor changes that indicate a theme color change may be needed from tab contents. */
@NullMarked
public class TabThemeColorHelper extends EmptyTabObserver {
    private final Callback<Integer> mUpdateCallback;

    TabThemeColorHelper(Tab tab, Callback<Integer> updateCallback) {
        mUpdateCallback = updateCallback;
        tab.addObserver(this);
    }

    /** Notifies the listeners of the tab theme color change. */
    private void updateIfNeeded(Tab tab, boolean didWebContentsThemeColorChange) {
        @ColorInt int themeColor = tab.getThemeColor();
        if (didWebContentsThemeColorChange) {
            WebContents webContents = tab.getWebContents();
            if (webContents != null) {
                themeColor = webContents.getThemeColor();
            }
        }
        mUpdateCallback.onResult(themeColor);
    }

    // TabObserver

    @Override
    public void onSSLStateUpdated(Tab tab) {
        updateIfNeeded(tab, false);
    }

    @Override
    public void onUrlUpdated(Tab tab) {
        updateIfNeeded(tab, false);
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        if (navigation.errorCode() != NetError.OK) updateIfNeeded(tab, true);
    }

    @Override
    public void onDestroyed(Tab tab) {
        tab.removeObserver(this);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.
    }
}
