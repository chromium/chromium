// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.annotation.RequiresApi;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;

/**
 * A ScrollCaptureManager is responsible for providing snapshots of the active tab to be used for
 * long screenshots.
 */
public class ScrollCaptureManager extends EmptyTabObserver implements Callback<Tab> {
    private final ObservableSupplier<Tab> mTabSupplier;
    private final ScrollCaptureManagerDelegate mDelegate;
    private Tab mCurrentTab;
    private View mCurrentView;

    @RequiresApi(api = VERSION_CODES.S)
    public ScrollCaptureManager(ObservableSupplier<Tab> tabSupplier) {
        this(tabSupplier, new ScrollCaptureManagerDelegateImpl());
    }

    ScrollCaptureManager(
            ObservableSupplier<Tab> tabSupplier, ScrollCaptureManagerDelegate delegate) {
        mTabSupplier = tabSupplier;
        mDelegate = delegate;
        mTabSupplier.addObserver(this);
    }

    @Override
    public void onResult(Tab tab) {
        if (mCurrentTab != null) {
            mCurrentTab.removeObserver(this);
        }
        mCurrentTab = tab;
        mDelegate.setCurrentTab(tab);
        if (mCurrentTab != null) {
            mCurrentTab.addObserver(this);
            onContentChanged(tab);
        }
    }

    @Override
    public void onContentChanged(Tab tab) {
        if (mCurrentView != null) {
            removeScrollCaptureBindings(mCurrentView);
            mCurrentView = null;
        }

        if (tab.isNativePage() || tab.isShowingCustomView() || tab.isShowingErrorPage()) return;

        mCurrentView = tab.getView();
        if (mCurrentView != null) {
            addScrollCaptureBindings(mCurrentView);
        }
    }

    private void addScrollCaptureBindings(View view) {
        mDelegate.addScrollCaptureBindings(view);
    }

    private void removeScrollCaptureBindings(View view) {
        mDelegate.removeScrollCaptureBindings(view);
    }

    public void destroy() {
        if (mTabSupplier != null) mTabSupplier.removeObserver(this);
        if (mCurrentTab != null) mCurrentTab.removeObserver(this);
        if (mCurrentView != null) removeScrollCaptureBindings(mCurrentView);
    }
}
