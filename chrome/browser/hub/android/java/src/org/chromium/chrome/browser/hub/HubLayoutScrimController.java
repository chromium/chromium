// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Wrapper around {@link ScrimCoordinator} to inject into {@link HubLayout}. */
public class HubLayoutScrimController implements ScrimController {
    // From TabSwitcherLayout.java.
    private static final int SCRIM_FADE_DURATION_MS = 350;

    private final @NonNull ScrimCoordinator mScrimCoordinator;
    private final @NonNull Supplier<View> mAnchorViewSupplier;
    private final @NonNull ObservableSupplier<Boolean> mIsIncognitoSupplier;
    private final @NonNull Callback<Boolean> mOnIncognitoChange = this::onIncognitoChange;

    private @Nullable PropertyModel mPropertyModel;

    /**
     * @param scrimCoordinator The {@link ScrimCoordinator} to use.
     * @param anchorViewSupplier The supplier for a {@link View} to anchor the scrim to. This must
     *     not return null if a scrim is going to be shown.
     * @param isIncognitoSupplier For checking and observing if the current UI is incognito.
     */
    public HubLayoutScrimController(
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull Supplier<View> anchorViewSupplier,
            @NonNull ObservableSupplier<Boolean> isIncognitoSupplier) {
        mScrimCoordinator = scrimCoordinator;
        mAnchorViewSupplier = anchorViewSupplier;
        mIsIncognitoSupplier = isIncognitoSupplier;
    }

    @Override
    public void startShowingScrim() {
        View anchorView = mAnchorViewSupplier.get();
        assert anchorView != null;

        PropertyModel.Builder scrimPropertiesBuilder =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.ANCHOR_VIEW, anchorView)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                        .with(ScrimProperties.BACKGROUND_COLOR, calculateScrimColor());

        mPropertyModel = scrimPropertiesBuilder.build();
        mIsIncognitoSupplier.addObserver(mOnIncognitoChange);
        mScrimCoordinator.showScrim(mPropertyModel);
    }

    @Override
    public void startHidingScrim() {
        if (!mScrimCoordinator.isShowingScrim()) return;

        mPropertyModel = null;
        mIsIncognitoSupplier.removeObserver(mOnIncognitoChange);
        mScrimCoordinator.hideScrim(true, SCRIM_FADE_DURATION_MS);
    }

    /** Forces the current animation to finish. */
    public void forceAnimationToFinish() {
        mScrimCoordinator.forceAnimationToFinish();
    }

    private void onIncognitoChange(Boolean ignored) {
        mPropertyModel.set(ScrimProperties.BACKGROUND_COLOR, calculateScrimColor());
    }

    private @ColorInt int calculateScrimColor() {
        View anchorView = mAnchorViewSupplier.get();
        assert anchorView != null;
        return ChromeColors.getPrimaryBackgroundColor(
                anchorView.getContext(), mIsIncognitoSupplier.get());
    }
}
