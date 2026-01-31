// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_FADE_DURATION_MS;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;

import java.util.function.DoubleConsumer;

/** A base class for Pane implementations. */
@NullMarked
public abstract class PaneBase implements Pane {
    private final @PaneId int mPaneId;
    protected final Context mContext;
    protected final DoubleConsumer mOnToolbarAlphaChange;
    protected final FrameLayout mRootView;
    protected final SettableNullableObservableSupplier<DisplayButtonData>
            mReferenceButtonDataSupplier = ObservableSuppliers.createNullable();
    protected final SettableNonNullObservableSupplier<Boolean> mHubSearchEnabledStateSupplier =
            ObservableSuppliers.createNonNull(true);
    protected final SettableNonNullObservableSupplier<Boolean> mHubSearchBoxVisibilitySupplier =
            ObservableSuppliers.createNonNull(true);
    protected SettableNonNullObservableSupplier<Boolean> mManualSearchBoxAnimationSupplier =
            ObservableSuppliers.createNonNull(false);
    protected SettableNonNullObservableSupplier<Float> mSearchBoxVisibilityFractionSupplier =
            ObservableSuppliers.createNonNull(0.0f);

    protected @HubColorScheme int mColorScheme = HubColorScheme.DEFAULT;
    protected boolean mMenuButtonVisible;
    protected @Nullable MenuOrKeyboardActionHandler mMenuOrKeyboardActionHandler;

    protected PaneBase(@PaneId int paneId, Context context, DoubleConsumer onToolbarAlphaChange) {
        mPaneId = paneId;
        mContext = context;
        mOnToolbarAlphaChange = onToolbarAlphaChange;
        mRootView = new FrameLayout(context);
    }

    @Override
    public final @PaneId int getPaneId() {
        return mPaneId;
    }

    @Override
    public final ViewGroup getRootView() {
        return mRootView;
    }

    @Override
    public final @HubColorScheme int getColorScheme() {
        return mColorScheme;
    }

    @Override
    public final @Nullable MenuOrKeyboardActionHandler getMenuOrKeyboardActionHandler() {
        return mMenuOrKeyboardActionHandler;
    }

    @Override
    public final boolean getMenuButtonVisible() {
        return mMenuButtonVisible;
    }

    @Override
    public final NonNullObservableSupplier<Boolean> getHubSearchEnabledStateSupplier() {
        return mHubSearchEnabledStateSupplier;
    }

    @Override
    public final NonNullObservableSupplier<Boolean> getHubSearchBoxVisibilitySupplier() {
        return mHubSearchBoxVisibilitySupplier;
    }

    @Override
    public final NullableObservableSupplier<DisplayButtonData> getReferenceButtonDataSupplier() {
        return mReferenceButtonDataSupplier;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getManualSearchBoxAnimationSupplier() {
        return mManualSearchBoxAnimationSupplier;
    }

    @Override
    public NonNullObservableSupplier<Float> getSearchBoxVisibilityFractionSupplier() {
        return mSearchBoxVisibilityFractionSupplier;
    }

    @Override
    public void setPaneHubController(@Nullable PaneHubController paneHubController) {}

    @Override
    public MonotonicObservableSupplier<FullButtonData> getActionButtonDataSupplier() {
        return ObservableSuppliers.alwaysNull();
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHairlineVisibilitySupplier() {
        return ObservableSuppliers.alwaysFalse();
    }

    @Override
    public NullableObservableSupplier<View> getHubOverlayViewSupplier() {
        return ObservableSuppliers.alwaysNull();
    }

    @Override
    public @Nullable HubLayoutAnimationListener getHubLayoutAnimationListener() {
        return null;
    }

    @Override
    public HubLayoutAnimatorProvider createShowHubLayoutAnimatorProvider(
            HubContainerView hubContainerView) {
        return FadeHubLayoutAnimationFactory.createFadeInAnimatorProvider(
                hubContainerView, HUB_LAYOUT_FADE_DURATION_MS, mOnToolbarAlphaChange);
    }

    @Override
    public HubLayoutAnimatorProvider createHideHubLayoutAnimatorProvider(
            HubContainerView hubContainerView) {
        return FadeHubLayoutAnimationFactory.createFadeOutAnimatorProvider(
                hubContainerView, HUB_LAYOUT_FADE_DURATION_MS, mOnToolbarAlphaChange);
    }
}
