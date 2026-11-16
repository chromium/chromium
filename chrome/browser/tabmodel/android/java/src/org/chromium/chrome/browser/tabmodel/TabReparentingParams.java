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

    /** Basic constructor for {@link TabReparentingParams}. */
    public TabReparentingParams(Tab tabToReparent, @Nullable Runnable finalizeCallback) {
        this(tabToReparent, /* sourceSelector= */ null, finalizeCallback);
    }

    /**
     * Constructor for {@link TabReparentingParams} when detaching from a source {@link
     * TabModelSelector} is deferred until reparenting finishes or the params are destroyed.
     */
    public TabReparentingParams(
            Tab tabToReparent,
            @Nullable TabModelSelector sourceSelector,
            @Nullable Runnable finalizeCallback) {
        mTabToReparent = tabToReparent;
        mSourceSelector = sourceSelector != null ? new WeakReference<>(sourceSelector) : null;
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
        if (mSourceSelector == null) return mFinalizeCallback;
        return () -> {
            if (mFinalizeCallback != null) mFinalizeCallback.run();
            detachFromOriginalModel();
        };
    }

    /** Detaches the tab from its source {@link TabModelSelector}, if still pending. */
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
