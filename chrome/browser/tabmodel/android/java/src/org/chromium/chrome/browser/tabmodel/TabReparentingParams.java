// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.ComponentName;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/** Class for handling tab reparenting operations across multiple activities. */
public class TabReparentingParams implements AsyncTabParams {
    private final Tab mTabToReparent;
    private final Runnable mFinalizeCallback;

    /** Basic constructor for {@link TabReparentingParams}. */
    public TabReparentingParams(Tab tabToReparent, Runnable finalizeCallback) {
        mTabToReparent = tabToReparent;
        mFinalizeCallback = finalizeCallback;
    }

    @Override
    public LoadUrlParams getLoadUrlParams() {
        return null;
    }

    @Override
    public Integer getRequestId() {
        return null;
    }

    @Override
    public WebContents getWebContents() {
        return null;
    }

    @Override
    public ComponentName getComponentName() {
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
