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

import java.lang.ref.WeakReference;

/** Class for handling tab reparenting operations across multiple activities. */
@NullMarked
public class TabReparentingParams implements AsyncTabParams {
    private final Tab mTabToReparent;
    private final @Nullable WeakReference<TabModelSelector> mSourceSelector;
    private final @Nullable Runnable mFinalizeCallback;

    /**
     * @param tabToReparent The tab being moved to another activity.
     * @param sourceSelector The {@link TabModelSelector} the tab is still attached to. Non-null
     *     only when that selector is in reparenting mode (activity recreation; see {@link
     *     TabModelSelector#isReparentingInProgress()}), where removing the tab from its model is
     *     deferred until reparenting finishes or these params are destroyed. Null in all other
     *     cases, where the selector removes the tab from its model as soon as it is detached.
     * @param finalizeCallback Run once reparenting has finished. Null if there is nothing to do.
     */
    public TabReparentingParams(
            Tab tabToReparent,
            @Nullable TabModelSelector sourceSelector,
            @Nullable Runnable finalizeCallback) {
        mTabToReparent = tabToReparent;
        mSourceSelector = sourceSelector != null ? new WeakReference<>(sourceSelector) : null;
        mFinalizeCallback =
                sourceSelector == null
                        ? finalizeCallback
                        : () -> {
                            if (finalizeCallback != null) finalizeCallback.run();
                            detachFromOriginalModel();
                        };
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

    @Override
    public @Nullable Runnable getFinalizeCallback() {
        return mFinalizeCallback;
    }

    @Override
    public void detachFromOriginalModel() {
        if (mSourceSelector == null) return;
        TabModelSelector selector = mSourceSelector.get();
        mSourceSelector.clear();
        if (selector != null) {
            selector.detachTabForReparenting(mTabToReparent);
        }
    }

    @Override
    public void destroy() {
        detachFromOriginalModel();
        if (mTabToReparent != null) mTabToReparent.destroy();
    }
}
