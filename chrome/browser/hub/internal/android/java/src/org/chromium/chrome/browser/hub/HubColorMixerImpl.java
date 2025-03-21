// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.HUB_CLOSED;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.HUB_SHOWN;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.TRANSLATE_DOWN_TABLET_ANIMATION_START;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.TRANSLATE_UP_TABLET_ANIMATION_END;

import android.animation.AnimatorSet;
import android.content.Context;
import android.graphics.Color;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.animation.AnimationHandler;

@NullMarked
public class HubColorMixerImpl implements HubColorMixer {

    private final ObservableSupplierImpl<@Nullable Integer> mOverviewColorSupplier =
            new ObservableSupplierImpl<>();
    private final Callback<Boolean> mOnHubVisibilityObserver = this::onHubVisibilityChange;
    private final Callback<Pane> mOnFocusedPaneObserver = this::onFocusedPaneChange;
    private final ObservableSupplier<Boolean> mHubVisibilitySupplier;
    private final ObservableSupplier<Pane> mFocusedPaneSupplier;
    private final HubColorBlendAnimatorSetHelper mAnimatorSetBuilder;
    private final AnimationHandler mColorBlendAnimatorHandler;
    private @Nullable HubColorSchemeUpdate mColorSchemeUpdate;

    /**
     * @param context The context for the Hub.
     * @param hubVisibilitySupplier Provides the Hub visibility.
     * @param focusedPaneSupplier Provides the currently focused {@link Pane}.
     */
    public HubColorMixerImpl(
            Context context,
            ObservableSupplier<Boolean> hubVisibilitySupplier,
            ObservableSupplier<Pane> focusedPaneSupplier) {
        mHubVisibilitySupplier = hubVisibilitySupplier;
        mFocusedPaneSupplier = focusedPaneSupplier;

        mHubVisibilitySupplier.addObserver(mOnHubVisibilityObserver);
        mFocusedPaneSupplier.addObserver(mOnFocusedPaneObserver);
        mColorBlendAnimatorHandler = new AnimationHandler();

        mAnimatorSetBuilder = new HubColorBlendAnimatorSetHelper();
        mAnimatorSetBuilder.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> HubColors.getBackgroundColor(context, colorScheme),
                        mOverviewColorSupplier::set));
    }

    @Override
    public void destroy() {
        mHubVisibilitySupplier.removeObserver(mOnHubVisibilityObserver);
        mFocusedPaneSupplier.removeObserver(mOnFocusedPaneObserver);
    }

    @Override
    public ObservableSupplierImpl<@Nullable Integer> getOverviewColorSupplier() {
        return mOverviewColorSupplier;
    }

    @Override
    public void processStateChange(@StateChange int colorChangeReason) {
        switch (colorChangeReason) {
            case HUB_SHOWN, TRANSLATE_DOWN_TABLET_ANIMATION_START -> enableOverviewMode();
            case TRANSLATE_UP_TABLET_ANIMATION_END, HUB_CLOSED -> disableOverviewMode();
            default -> throw new IllegalStateException("Unexpected value: " + colorChangeReason);
        }
    }

    @Override
    public void registerBlend(HubViewColorBlend colorBlend) {
        mAnimatorSetBuilder.registerBlend(colorBlend);
        if (mColorSchemeUpdate != null) {
            colorBlend
                    .createAnimationForTransition(
                            mColorSchemeUpdate.newColorScheme,
                            mColorSchemeUpdate.previousColorScheme)
                    .start();
        }
    }

    private void onFocusedPaneChange(@Nullable Pane focusedPane) {
        @HubColorScheme int newColorScheme = HubColors.getColorSchemeSafe(focusedPane);
        updateColorScheme(newColorScheme);
    }

    private void updateColorScheme(@HubColorScheme int newColorScheme) {
        @HubColorScheme
        int prevColorScheme =
                mColorSchemeUpdate == null ? newColorScheme : mColorSchemeUpdate.newColorScheme;

        AnimatorSet animatorSet =
                mAnimatorSetBuilder
                        .setNewColorScheme(newColorScheme)
                        .setPreviousColorScheme(prevColorScheme)
                        .build();
        mColorSchemeUpdate = new HubColorSchemeUpdate(newColorScheme, prevColorScheme);
        mColorBlendAnimatorHandler.startAnimation(animatorSet);
    }

    private void onHubVisibilityChange(boolean isVisible) {
        processStateChange(isVisible ? HUB_SHOWN : HUB_CLOSED);
    }

    private void disableOverviewMode() {
        mOverviewColorSupplier.set(Color.TRANSPARENT);
        mColorSchemeUpdate = null;
    }

    private void enableOverviewMode() {
        onFocusedPaneChange(mFocusedPaneSupplier.get());
    }
}
