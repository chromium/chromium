// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.build.annotations.MockedInTests;

/**
 */
@MockedInTests
public interface TabLifecycle {
    /**
     * @return Whether or not this Tab has a live native component.  This will be true prior to
     *         {@link #initializeNative()} being called or after {@link #destroy()}.
     */
    boolean isInitialized();

    /**
     * @return Whether this Tab has been destroyed.
     */
    boolean isDestroyed();

    /**
     * Prepares the tab to be shown. This method is supposed to be called before the tab is
     * displayed. It restores the ContentView if it is not available after the cold start and
     * reloads the tab if its renderer has crashed.
     *
     * @param type Specifies how the tab was selected.
     * @param caller The caller of this method.
     */
    void show(@TabSelectionType int type, @TabLoadIfNeededCaller int caller);

    /** Triggers the hiding logic for the view backing the tab. */
    void hide(@TabHidingType int type);

    /**
     * @return Whether or not the tab is in the closing process.
     * TODO(jinsukkim): isClosing/setClosing are used by TabModel only. Consider removing
     *     them from this interface.
     */
    boolean isClosing();

    /**
     * @param closing Whether or not the tab is in the closing process.
     */
    void setClosing(boolean closing);

    /**
     * @return Whether or not the tab is hidden.
     */
    boolean isHidden();

    /**
     * Cleans up all internal state, destroying any {@link NativePage} or {@link WebContents}
     * currently associated with this {@link Tab}.  This also destroys the native counterpart
     * to this class, which means that all subclasses should erase their native pointers after
     * this method is called.  Once this call is made this {@link Tab} should no longer be used.
     */
    void destroy();
}
