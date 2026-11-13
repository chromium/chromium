// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** UserData for a {@link Tab}. Used for a {@link WebContents} while it stays active for the Tab. */
@NullMarked
public abstract class TabWebContentsUserData implements TabObserver, UserData {
    private final Tab mTab;
    private @Nullable WebContents mWebContents;

    /**
     * Constructs a new {@link TabWebContentsUserData}.
     *
     * <p>Subclasses should register new instances via {@link #attachToTab(Class,
     * TabWebContentsUserData)} rather than calling {@link UserDataHost#setUserData} directly so any
     * active {@link WebContents} is bound after subclass construction completes (aside from
     * flag-gated helpers such as {@code TabFavicon}).
     */
    protected TabWebContentsUserData(Tab tab) {
        assert !tab.isDestroyed();
        mTab = tab;
        mTab.addObserver(this);
    }

    /**
     * Attaches {@code userData} to its {@link Tab}'s {@link UserDataHost} and binds any active
     * {@link WebContents} after subclass construction has completed.
     */
    protected static <T extends TabWebContentsUserData> T attachToTab(Class<T> key, T userData) {
        Tab tab = ((TabWebContentsUserData) userData).mTab;
        UserDataHost host = tab.getUserDataHost();
        assert host.getUserData(key) == null;
        host.setUserData(key, userData);
        userData.onContentChanged(tab);
        return userData;
    }

    @Override
    public final void onContentChanged(Tab tab) {
        WebContents newWebContents = tab.getWebContents();
        if (mWebContents == newWebContents) {
            return;
        }
        WebContents oldWebContents = mWebContents;
        mWebContents = null;
        if (oldWebContents != null) {
            cleanupWebContents(oldWebContents);
        }
        mWebContents = newWebContents;
        if (mWebContents != null) {
            initWebContents(mWebContents);
        }
    }

    @Override
    public void onDestroyed(Tab tab) {
        tab.removeObserver(this);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on
        // detachment.
    }

    @Override
    public final void destroy() {
        WebContents oldWebContents = mWebContents;
        mWebContents = null;
        if (oldWebContents != null) {
            cleanupWebContents(oldWebContents);
        }
        destroyInternal();
        mTab.removeObserver(this);
    }

    protected @Nullable WebContents getWebContents() {
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
     *
     * @param webContents WebContents object that just became inactive.
     */
    public abstract void cleanupWebContents(WebContents webContents);
}
