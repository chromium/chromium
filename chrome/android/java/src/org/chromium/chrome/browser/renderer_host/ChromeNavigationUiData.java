// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.renderer_host;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.function.LongSupplier;

/** Provides a way to attach chrome-specific navigation ui data from java. */
@NullMarked
public class ChromeNavigationUiData implements LongSupplier {
    private @Nullable Long mBookmarkId;

    private ChromeNavigationUiData() {}

    /**
     * Safely extracts or initializes the ChromeNavigationUiData payload attached to LoadUrlParams.
     */
    public static ChromeNavigationUiData getOrCreate(LoadUrlParams params) {
        LongSupplier existing = params.getNavigationUIDataSupplier();
        if (existing instanceof ChromeNavigationUiData chromeNavData) {
            return chromeNavData;
        }
        assert existing == null
                : "LoadUrlParams already has a different NavigationUIData supplier set!";
        var newData = new ChromeNavigationUiData();
        params.setNavigationUIDataSupplier(newData);
        return newData;
    }

    /** Set the bookmark id on this navigation. */
    public ChromeNavigationUiData setBookmarkId(long bookmarkId) {
        mBookmarkId = bookmarkId;
        return this;
    }

    @Override
    public long getAsLong() {
        return ChromeNavigationUiDataJni.get().create(mBookmarkId);
    }

    @NativeMethods
    interface Natives {
        long create(@JniType("std::optional<int64_t>") @Nullable Long bookmarkId);
    }
}
