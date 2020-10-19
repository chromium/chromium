// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
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
    private final List<Callback<LayoutManager>> mListeners = new ArrayList<>();

    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final Activity mActivity;
    private final Lazy<CompositorViewHolder> mCompositorViewHolder;
    private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final CompositorViewHolder.Initializer mCompositorViewHolderInitializer;

    private boolean mInitialized;

    @Inject
    public CustomTabCompositorContentInitializer(ActivityLifecycleDispatcher lifecycleDispatcher,
            Activity activity, Lazy<CompositorViewHolder> compositorViewHolder,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            CompositorViewHolder.Initializer compositorViewHolderInitializer) {
        mLifecycleDispatcher = lifecycleDispatcher;
        mActivity = activity;
        mCompositorViewHolder = compositorViewHolder;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mCompositorViewHolderInitializer = compositorViewHolderInitializer;

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
        ViewGroup contentContainer = mActivity.findViewById(android.R.id.content);
        LayoutManager layoutDriver = new LayoutManager(
                mCompositorViewHolder.get(), contentContainer, mTabContentManagerSupplier);

        mCompositorViewHolderInitializer.initializeCompositorContent(layoutDriver,
                mActivity.findViewById(org.chromium.chrome.R.id.url_bar), contentContainer,
                mActivity.findViewById(org.chromium.chrome.R.id.control_container));

        for (Callback<LayoutManager> listener : mListeners) {
            listener.onResult(layoutDriver);
        }

        mInitialized = true;
        mListeners.clear();
        mLifecycleDispatcher.unregister(this);
    }
}
