// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.ComponentName;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/** Class for handling tab reparenting operations across multiple activities. */
@NullMarked
public class TabReparentingParams implements AsyncTabParams {
    private final Tab mTabToReparent;
    private final @Nullable Runnable mFinalizeCallback;

    /** Basic constructor for {@link TabReparentingParams}. */
    public TabReparentingParams(Tab tabToReparent, @Nullable Runnable finalizeCallback) {
        mTabToReparent = tabToReparent;
        mFinalizeCallback = finalizeCallback;
    }

    @Override
    public @Nullable LoadUrlParams getLoadUrlParams() {
        return null;
    }

    @Override
    public @Nullable Integer getRequestId() {
        return null;
    }

    @Override
    public @Nullable WebContents getWebContents() {
        return null;
    }

    @Override
    public @Nullable ComponentName getComponentName() {
        return null;
    }

    @Override
    public Tab getTabToReparent() {
        return mTabToReparent;
    }

    /** Returns the callback to be used once Tab reparenting has finished, if any. */
    public @Nullable Runnable getFinalizeCallback() {
        return mFinalizeCallback;
    }

    @Override
    public void destroy() {
        if (mTabToReparent != null) mTabToReparent.destroy();
    }
}
