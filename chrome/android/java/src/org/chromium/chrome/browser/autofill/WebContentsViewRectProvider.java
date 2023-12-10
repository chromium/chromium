// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.graphics.Rect;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.widget.RectProvider;

/**
 * Helper class that computes the visible rect of a given {@link WebContents} objects. It observes
 * the given {@link WebContents} object to update the observed suppliers of browser controls and
 * filling component. Use like a standard {@link RectProvider}.
 *
 * Examples for observed changes:
 * - {@link BrowserControlsManager} providing height for elements like the Omnibox.
 * - {@link ManualFillingComponent} providing filling surface height like Keyboard Accessory.
 * - A Bottom Inset supplier for the Soft-keyboard.
 */
class WebContentsViewRectProvider extends RectProvider {
    private final WebContents mWebContents;
    private ObservableSupplier<BrowserControlsManager> mBrowserControlsSupplier;
    private ObservableSupplier<ManualFillingComponent> mManualFillingComponentSupplier;
    private ObservableSupplier<Integer> mBottomInsetSupplier;

    private final Callback<ManualFillingComponent> mOnManualFillingComponentChanged =
            fillComponent -> observeBottomInsetSupplier(fillComponent.getBottomInsetSupplier());
    private final Callback<Integer> mOnBottomInsetChanged =
            bottomInset ->
                    updateVisibleRectForPopup(
                            bottomInset, getValueOrNull(mBrowserControlsSupplier));
    private final Callback<BrowserControlsManager> mOnBrowserControlsChanged =
            ctrlMgr -> updateVisibleRectForPopup(getValueOrNull(mBottomInsetSupplier), ctrlMgr);

    /**
     * Creates a new RectProvider and starts observing given parameters. If they provide a valid
     * rect, it's immediately computed.
     *
     * @param webContents A required, non-null {@link WebContents} object.
     * @param browserControlsSupplier A {@link ObservableSupplier<BrowserControlsManager>}.
     * @param manualFillingComponentSupplier A {@link ObservableSupplier<ManualFillingComponent>}.
     */
    public WebContentsViewRectProvider(
            WebContents webContents,
            ObservableSupplier<BrowserControlsManager> browserControlsSupplier,
            ObservableSupplier<ManualFillingComponent> manualFillingComponentSupplier) {
        assert webContents != null;
        assert webContents.getViewAndroidDelegate() != null;
        assert webContents.getViewAndroidDelegate().getContainerView() != null;

        mWebContents = webContents;
        observeManualFillingComponentSupplier(manualFillingComponentSupplier);
        observeBrowserControlsSupplier(browserControlsSupplier);
    }

    /** Stops observing the suppliers given in the constructor. */
    public void dismiss() {
        observeManualFillingComponentSupplier(null);
        observeBrowserControlsSupplier(null);
    }

    private void observeBrowserControlsSupplier(
            @Nullable ObservableSupplier<BrowserControlsManager> supplier) {
        if (mBrowserControlsSupplier != null) {
            mBrowserControlsSupplier.removeObserver(mOnBrowserControlsChanged);
        }
        mBrowserControlsSupplier = supplier;
        if (mBrowserControlsSupplier != null) {
            mBrowserControlsSupplier.addObserver(mOnBrowserControlsChanged);
        }
        updateVisibleRectForPopup(
                getValueOrNull(mBottomInsetSupplier), getValueOrNull(mBrowserControlsSupplier));
    }

    private void observeManualFillingComponentSupplier(
            @Nullable ObservableSupplier<ManualFillingComponent> supplier) {
        if (mManualFillingComponentSupplier != null) {
            observeBottomInsetSupplier(null);
            mManualFillingComponentSupplier.removeObserver(mOnManualFillingComponentChanged);
        }
        mManualFillingComponentSupplier = supplier;
        if (mManualFillingComponentSupplier != null) {
            mManualFillingComponentSupplier.addObserver(mOnManualFillingComponentChanged);
            observeBottomInsetSupplier(
                    mManualFillingComponentSupplier.hasValue()
                            ? mManualFillingComponentSupplier.get().getBottomInsetSupplier()
                            : null);
        }
    }

    private void observeBottomInsetSupplier(@Nullable ObservableSupplier<Integer> supplier) {
        if (mBottomInsetSupplier != null) {
            mBottomInsetSupplier.removeObserver(mOnBottomInsetChanged);
        }
        mBottomInsetSupplier = supplier;
        if (mBottomInsetSupplier != null) {
            mBottomInsetSupplier.addObserver(mOnBottomInsetChanged);
        }
        updateVisibleRectForPopup(
                getValueOrNull(mBottomInsetSupplier), getValueOrNull(mBrowserControlsSupplier));
    }

    private void updateVisibleRectForPopup(
            @Nullable Integer bottomInset, @Nullable BrowserControlsManager browserControls) {
        ViewAndroidDelegate viewDelegate = mWebContents.getViewAndroidDelegate();

        // Stop computing a rect if the WebContents view is gone for some reason.
        if (viewDelegate == null || viewDelegate.getContainerView() == null) return;

        Rect rect = new Rect();
        viewDelegate.getContainerView().getGlobalVisibleRect(rect);
        if (browserControls != null) {
            rect.top += browserControls.getTopControlsHeight();
            rect.bottom -= browserControls.getBottomControlOffset();
        }
        if (bottomInset != null) rect.bottom -= bottomInset;

        if (!mRect.equals(rect)) setRect(rect); // Update and notify only if the rect changes.
    }

    private static @Nullable <T> T getValueOrNull(@Nullable Supplier<T> supplier) {
        return supplier != null ? supplier.get() : null;
    }
}
