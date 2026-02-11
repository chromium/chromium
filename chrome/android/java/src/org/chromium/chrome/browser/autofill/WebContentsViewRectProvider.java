// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.graphics.Rect;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
 * <pre>
 * Examples for observed changes:
 * - {@link BrowserControlsManager} providing height for elements like the Omnibox.
 * - {@link ManualFillingComponent} providing filling surface height like Keyboard Accessory.
 * - A Bottom Inset supplier for the Soft-keyboard.
 * </pre>
 */
@NullMarked
class WebContentsViewRectProvider extends RectProvider {
    private final WebContents mWebContents;
    private @Nullable MonotonicObservableSupplier<BrowserControlsManager> mBrowserControlsSupplier;
    private @Nullable MonotonicObservableSupplier<ManualFillingComponent>
            mManualFillingComponentSupplier;
    private @Nullable NonNullObservableSupplier<Integer> mBottomInsetSupplier;

    private final Callback<ManualFillingComponent> mOnManualFillingComponentChanged =
            fillComponent -> observeBottomInsetSupplier(fillComponent.getBottomInsetSupplier());
    private final Callback<Integer> mOnBottomInsetChanged =
            bottomInset ->
                    updateVisibleRectForPopup(
                            bottomInset, SupplierUtils.getOrNull(mBrowserControlsSupplier));
    private final Callback<BrowserControlsManager> mOnBrowserControlsChanged =
            ctrlMgr ->
                    updateVisibleRectForPopup(
                            SupplierUtils.getOrNull(mBottomInsetSupplier), ctrlMgr);

    /**
     * Creates a new RectProvider and starts observing given parameters. If they provide a valid
     * rect, it's immediately computed.
     *
     * @param webContents A required, non-null {@link WebContents} object.
     * @param browserControlsSupplier A {@link MonotonicObservableSupplier
     *     <BrowserControlsManager>}.
     * @param manualFillingComponentSupplier A {@link MonotonicObservableSupplier
     *     <ManualFillingComponent>}.
     */
    public WebContentsViewRectProvider(
            WebContents webContents,
            MonotonicObservableSupplier<BrowserControlsManager> browserControlsSupplier,
            MonotonicObservableSupplier<ManualFillingComponent> manualFillingComponentSupplier) {
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
            @Nullable MonotonicObservableSupplier<BrowserControlsManager> supplier) {
        if (mBrowserControlsSupplier != null) {
            mBrowserControlsSupplier.removeObserver(mOnBrowserControlsChanged);
        }
        mBrowserControlsSupplier = supplier;
        if (mBrowserControlsSupplier != null) {
            mBrowserControlsSupplier.addSyncObserverAndPostIfNonNull(mOnBrowserControlsChanged);
        }
        updateVisibleRectForPopup(
                SupplierUtils.getOrNull(mBottomInsetSupplier),
                SupplierUtils.getOrNull(mBrowserControlsSupplier));
    }

    private void observeManualFillingComponentSupplier(
            @Nullable MonotonicObservableSupplier<ManualFillingComponent> supplier) {
        if (mManualFillingComponentSupplier != null) {
            observeBottomInsetSupplier(null);
            mManualFillingComponentSupplier.removeObserver(mOnManualFillingComponentChanged);
        }
        mManualFillingComponentSupplier = supplier;
        if (mManualFillingComponentSupplier != null) {
            mManualFillingComponentSupplier.addSyncObserverAndCallIfNonNull(
                    mOnManualFillingComponentChanged);
        }
    }

    private void observeBottomInsetSupplier(@Nullable NonNullObservableSupplier<Integer> supplier) {
        if (mBottomInsetSupplier != null) {
            mBottomInsetSupplier.removeObserver(mOnBottomInsetChanged);
        }
        mBottomInsetSupplier = supplier;
        if (mBottomInsetSupplier != null) {
            mBottomInsetSupplier.addSyncObserverAndPostIfNonNull(mOnBottomInsetChanged);
        }
        updateVisibleRectForPopup(
                SupplierUtils.getOrNull(mBottomInsetSupplier),
                SupplierUtils.getOrNull(mBrowserControlsSupplier));
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
}
