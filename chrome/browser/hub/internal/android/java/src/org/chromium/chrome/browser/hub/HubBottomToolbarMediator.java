// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubBottomToolbarProperties.BOTTOM_TOOLBAR_VISIBLE;
import static org.chromium.chrome.browser.hub.HubBottomToolbarProperties.COLOR_SCHEME;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for the Hub bottom toolbar. This class handles the business logic and state management
 * for the bottom toolbar component.
 */
@NullMarked
public class HubBottomToolbarMediator {
    private final PropertyModel mPropertyModel;
    private final HubBottomToolbarDelegate mDelegate;
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final NonNullObservableSupplier<Boolean> mIsHidingSupplier;
    private final Callback<Boolean> mOnVisibilityChange = this::onVisibilityChange;
    private final Callback<@Nullable Tab> mOnCurrentTabChanged = this::onCurrentTabChanged;
    private final Callback<Boolean> mOnHidingChanged = this::onHidingChanged;

    /**
     * Creates a new HubBottomToolbarMediator.
     *
     * @param propertyModel The property model to update with toolbar state.
     * @param delegate The delegate that provides visibility information.
     */
    public HubBottomToolbarMediator(
            PropertyModel propertyModel, HubBottomToolbarDelegate delegate) {
        this(
                propertyModel,
                delegate,
                ObservableSuppliers.alwaysNull(),
                ObservableSuppliers.alwaysFalse());
    }

    /**
     * Creates a new HubBottomToolbarMediator.
     *
     * @param propertyModel The property model to update with toolbar state.
     * @param delegate The delegate that provides visibility information.
     * @param currentTabSupplier The supplier of the current tab.
     * @param isHidingSupplier Supplies whether the Hub is currently hiding / exiting.
     */
    public HubBottomToolbarMediator(
            PropertyModel propertyModel,
            HubBottomToolbarDelegate delegate,
            NullableObservableSupplier<Tab> currentTabSupplier,
            NonNullObservableSupplier<Boolean> isHidingSupplier) {
        mPropertyModel = propertyModel;
        mDelegate = delegate;
        mCurrentTabSupplier = currentTabSupplier;
        mIsHidingSupplier = isHidingSupplier;

        mDelegate
                .getBottomToolbarVisibilitySupplier()
                .addSyncObserverAndCallIfNonNull(mOnVisibilityChange);
        mCurrentTabSupplier.addSyncObserverAndCallIfNonNull(mOnCurrentTabChanged);
        mIsHidingSupplier.addSyncObserverAndCallIfNonNull(mOnHidingChanged);
    }

    /** Cleans up observers and unregisters callbacks. */
    public void destroy() {
        mDelegate.getBottomToolbarVisibilitySupplier().removeObserver(mOnVisibilityChange);
        mCurrentTabSupplier.removeObserver(mOnCurrentTabChanged);
        mIsHidingSupplier.removeObserver(mOnHidingChanged);
    }

    private void onVisibilityChange(Boolean visible) {
        mPropertyModel.set(BOTTOM_TOOLBAR_VISIBLE, visible);
    }

    private void onHidingChanged(Boolean isHiding) {
        // Synchronize with the destination tab's color scheme for the exit animation.
        // We do not reset on `isHiding = false` so the tab's colors are preserved when
        // exit completes (`doneHiding()`), while active Hub colors are managed by HubColorMixer.
        if (Boolean.TRUE.equals(isHiding)) {
            updateColorSchemeForTab(mCurrentTabSupplier.get());
        }
    }

    private void onCurrentTabChanged(@Nullable Tab tab) {
        if (!Boolean.TRUE.equals(mIsHidingSupplier.get())) {
            return;
        }
        updateColorSchemeForTab(tab);
    }

    private void updateColorSchemeForTab(@Nullable Tab tab) {
        @HubColorScheme int colorScheme = HubColors.getColorSchemeForTab(tab);
        mPropertyModel.set(COLOR_SCHEME, colorScheme);
    }
}
