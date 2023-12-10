// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.renderer_host;

import org.jni_zero.NativeMethods;

/** Provides a way to attach chrome-specific navigation ui data from java. */
public class ChromeNavigationUIData {
    private long mBookmarkId;

    /**
     * Reconstructs the native NavigationUIData for this Java NavigationUIData, returning its
     * native pointer and transferring ownership to the calling function.
     */
    public long createUnownedNativeCopy() {
        return ChromeNavigationUIDataJni.get()
                .createUnownedNativeCopy(ChromeNavigationUIData.this, mBookmarkId);
    }

    /** Set the bookmark id on this navigation. */
    public void setBookmarkId(long bookmarkId) {
        mBookmarkId = bookmarkId;
    }

    @NativeMethods
    interface Navites {
        long createUnownedNativeCopy(ChromeNavigationUIData caller, long bookmarkId);
    }
}
