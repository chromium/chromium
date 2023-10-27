// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Wrapper around {@link ScrimCoordinator} to inject into {@link HubLayout}. */
public class HubLayoutScrimController implements ScrimController {
    // From TabSwitcherLayout.java.
    private static final int SCRIM_FADE_DURATION_MS = 350;

    private final ScrimCoordinator mScrimCoordinator;
    private final View mAnchorView;
    private final Supplier<Boolean> mIsIncognitoSupplier;

    /**
     * @param scrimCoordinator The {@link ScrimCoordinator} to use.
     * @param anchorView The {@link View} to anchor the scrim to.
     * @param isIncognitoSupplier For checking if the current UI is incognito.
     */
    public HubLayoutScrimController(
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull View anchorView,
            @NonNull Supplier<Boolean> isIncognitoSupplier) {
        mScrimCoordinator = scrimCoordinator;
        mAnchorView = anchorView;
        mIsIncognitoSupplier = isIncognitoSupplier;
    }

    @Override
    public void startShowingScrim() {
        @ColorInt
        int scrimColor =
                ChromeColors.getPrimaryBackgroundColor(
                        mAnchorView.getContext(), mIsIncognitoSupplier.get());

        PropertyModel.Builder scrimPropertiesBuilder =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.ANCHOR_VIEW, mAnchorView)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                        .with(ScrimProperties.BACKGROUND_COLOR, scrimColor);

        mScrimCoordinator.showScrim(scrimPropertiesBuilder.build());
    }

    @Override
    public void startHidingScrim() {
        if (!mScrimCoordinator.isShowingScrim()) return;

        mScrimCoordinator.hideScrim(true, SCRIM_FADE_DURATION_MS);
    }

    /** Forces the current animation to finish. */
    public void forceAnimationToFinish() {
        mScrimCoordinator.forceAnimationToFinish();
    }
}
