// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;

/**
 * Class responsible for ensuring a web-exposed CloseWatcher is able to intercept a back gesture and
 * perform an app-specific behavior.
 */
public class CloseListenerManager implements BackPressHandler, Destroyable {
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplier<Tab> mActivityTabSupplier;
    private final Callback<Tab> mOnTabChanged = this::onTabChanged;
    private Tab mTab;
    private TabObserver mTabObserver;

    public CloseListenerManager(ObservableSupplier<Tab> activityTabSupplier) {
        mActivityTabSupplier = activityTabSupplier;
        mActivityTabSupplier.addObserver(mOnTabChanged);
        updateObserver();
    }

    @Override
    public @BackPressResult int handleBackPress() {
        RenderFrameHost focusedFrame = getFocusedFrameIfCloseWatcherActive();
        if (focusedFrame == null) return BackPressResult.FAILURE;
        focusedFrame.signalCloseWatcherIfActive();
        return BackPressResult.SUCCESS;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public void destroy() {
        mActivityTabSupplier.removeObserver(mOnTabChanged);
        if (mTabObserver != null) {
            mTab.removeObserver(mTabObserver);
            mTabObserver = null;
            mTab = null;
        }
    }

    private void onTabChanged(Tab tab) {
        onBackPressStateChanged();
        updateObserver();
    }

    private void updateObserver() {
        if (mTabObserver != null) {
            mTab.removeObserver(mTabObserver);
            mTabObserver = null;
        }

        mTab = mActivityTabSupplier.get();
        if (mTab == null) return;

        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onNavigationStateChanged() {
                        onBackPressStateChanged();
                    }

                    @Override
                    public void onDidChangeCloseSignalInterceptStatus() {
                        onBackPressStateChanged();
                    }
                };
        mTab.addObserver(mTabObserver);
    }

    private void onBackPressStateChanged() {
        mBackPressChangedSupplier.set(shouldInterceptBackPress());
    }

    private boolean shouldInterceptBackPress() {
        return getFocusedFrameIfCloseWatcherActive() != null;
    }

    private RenderFrameHost getFocusedFrameIfCloseWatcherActive() {
        Tab tab = mActivityTabSupplier.get();
        if (tab == null) return null;
        WebContents contents = tab.getWebContents();
        if (contents == null) return null;
        RenderFrameHost focusedFrame = contents.getFocusedFrame();
        if (focusedFrame == null || !focusedFrame.isCloseWatcherActive()) return null;
        return focusedFrame;
    }
}
