// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;

import java.util.ArrayList;
import java.util.List;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Initializes the compositor content (calls {@link ChromeActivity#initializeCompositorContent}).
 */
@ActivityScope
public class CustomTabCompositorContentInitializer implements NativeInitObserver {
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;

    private final ChromeActivity mActivity;
    private final Lazy<CompositorViewHolder> mCompositorViewHolder;

    private final List<Callback<LayoutManager>> mListeners = new ArrayList<>();
    private boolean mInitialized;

    @Inject
    public CustomTabCompositorContentInitializer(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            ChromeActivity activity,
            Lazy<CompositorViewHolder> compositorViewHolder) {
        mLifecycleDispatcher = lifecycleDispatcher;
        mActivity = activity;
        mCompositorViewHolder = compositorViewHolder;

        lifecycleDispatcher.register(this);
    }

    /**
     * Adds a callback that will be called once the Compositor View Holder has its content
     * initialized, or immediately (synchronously) if it is already initialized.
     */
    public void addCallback(Callback<LayoutManager> callback) {

        if (mInitialized) {
            callback.onResult(mCompositorViewHolder.get().getLayoutManager());
        } else {
            mListeners.add(callback);
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        LayoutManager layoutDriver = new LayoutManager(mCompositorViewHolder.get());
        mActivity.initializeCompositorContent(layoutDriver,
                mActivity.findViewById(org.chromium.chrome.R.id.url_bar),
                mActivity.findViewById(android.R.id.content),
                mActivity.findViewById(org.chromium.chrome.R.id.control_container));

        for (Callback<LayoutManager> listener : mListeners) {
            listener.onResult(layoutDriver);
        }

        mInitialized = true;
        mListeners.clear();
        mLifecycleDispatcher.unregister(this);
    }
}
