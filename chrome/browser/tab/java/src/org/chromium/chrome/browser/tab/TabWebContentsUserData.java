// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.base.UserData;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * UserData for a {@link Tab}. Used for a {@link WebContents} while it stays
 * active for the Tab.
 */
public abstract class TabWebContentsUserData implements UserData {
    private WebContents mWebContents;

    public TabWebContentsUserData(Tab tab) {
        tab.addObserver(
                new EmptyTabObserver() {
                    @Override
                    public void onContentChanged(Tab tab) {
                        if (mWebContents == tab.getWebContents()) return;
                        if (mWebContents != null) cleanupWebContents(mWebContents);
                        mWebContents = tab.getWebContents();
                        if (mWebContents != null) initWebContents(mWebContents);
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        tab.removeObserver(this);
                    }

                    @Override
                    public void onActivityAttachmentChanged(
                            Tab tab, @Nullable WindowAndroid window) {
                        // Intentionally do nothing to prevent automatic observer removal on
                        // detachment.
                    }
                });
    }

    @Override
    public final void destroy() {
        cleanupWebContents(mWebContents);
        destroyInternal();
    }

    protected WebContents getWebContents() {
        return mWebContents;
    }

    /** Performs additional tasks upon destruction. */
    protected void destroyInternal() {}

    /**
     * Called when {@link WebContents} becomes active (swapped in) for a {@link Tab}.
     * @param webContents WebContents object that just became active.
     */
    public abstract void initWebContents(WebContents webContents);

    /**
     * Called when {@link WebContents} gets swapped out.
     * @param webContents WebContents object that just became inactive.
     */
    public abstract void cleanupWebContents(WebContents webContents);
}
