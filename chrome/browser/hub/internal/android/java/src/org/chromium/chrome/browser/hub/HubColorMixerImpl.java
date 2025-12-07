// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.HUB_CLOSED;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.HUB_SHOWN;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.TRANSLATE_DOWN_TABLET_ANIMATION_START;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.TRANSLATE_UP_TABLET_ANIMATION_END;

import android.animation.AnimatorSet;
import android.content.Context;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.FloatRange;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.util.ColorUtils;

@NullMarked
public class HubColorMixerImpl implements HubColorMixer {
    /** Maps the Hub Color Scheme to the color for the hub overview color. */
    @FunctionalInterface
    @VisibleForTesting
    interface HubOverviewColorProvider extends HubViewColorBlend.ColorGetter {}

    private final ObservableSupplierImpl<Integer> mOverviewColorSupplier =
            new ObservableSupplierImpl<>(Color.TRANSPARENT);
    private final Callback<Boolean> mOnHubVisibilityObserver = this::onHubVisibilityChange;
    private final Callback<Pane> mOnFocusedPaneObserver = this::onFocusedPaneChange;
    private final ObservableSupplier<Boolean> mHubVisibilitySupplier;
    private final ObservableSupplier<Pane> mFocusedPaneSupplier;
    private final HubColorBlendAnimatorSetHelper mAnimatorSetBuilder;
    private final AnimationHandler mColorBlendAnimatorHandler;
    private final boolean mIsTablet;
    private @Nullable HubColorSchemeUpdate mColorSchemeUpdate;
    private float mOverviewColorAlpha;
    private boolean mOverviewMode;

    /**
     * @param context The context for the Hub.
     * @param hubVisibilitySupplier Provides the Hub visibility.
     * @param focusedPaneSupplier Provides the currently focused {@link Pane}.
     */
    public HubColorMixerImpl(
            Context context,
            ObservableSupplier<Boolean> hubVisibilitySupplier,
            ObservableSupplier<Pane> focusedPaneSupplier) {
        this(
                hubVisibilitySupplier,
                focusedPaneSupplier,
                new HubColorBlendAnimatorSetHelper(),
                new AnimationHandler(),
                colorScheme -> HubColors.getBackgroundColor(context, colorScheme),
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(context));
    }

    @VisibleForTesting
    HubColorMixerImpl(
            ObservableSupplier<Boolean> hubVisibilitySupplier,
            ObservableSupplier<Pane> focusedPaneSupplier,
            HubColorBlendAnimatorSetHelper animatorSetHelper,
            AnimationHandler animationHandler,
            HubOverviewColorProvider hubOverviewColorProvider,
            boolean isTablet) {
        mHubVisibilitySupplier = hubVisibilitySupplier;
        mFocusedPaneSupplier = focusedPaneSupplier;
        mColorBlendAnimatorHandler = animationHandler;
        mAnimatorSetBuilder = animatorSetHelper;
        mIsTablet = isTablet;

        mHubVisibilitySupplier.addObserver(mOnHubVisibilityObserver);
        mFocusedPaneSupplier.addObserver(mOnFocusedPaneObserver);

        mOverviewColorAlpha = 1f;
        disableOverviewMode();
        registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        hubOverviewColorProvider,
                        color -> processOverviewColor(color, mOverviewColorAlpha)));
    }

    @Override
    public void destroy() {
        mHubVisibilitySupplier.removeObserver(mOnHubVisibilityObserver);
        mFocusedPaneSupplier.removeObserver(mOnFocusedPaneObserver);
    }

    @Override
    public ObservableSupplier<Integer> getOverviewColorSupplier() {
        return mOverviewColorSupplier;
    }

    @Override
    public void processStateChange(@StateChange int colorChangeReason) {
        switch (colorChangeReason) {
            case HUB_SHOWN -> {
                if (mIsTablet) {
                    onFocusedPaneChange(mFocusedPaneSupplier.get());
                } else {
                    enableOverviewMode();
                }
            }
            case TRANSLATE_UP_TABLET_ANIMATION_END -> enableOverviewMode();
            case HUB_CLOSED -> {
                if (!mIsTablet) {
                    disableOverviewMode();
                }
            }
            case TRANSLATE_DOWN_TABLET_ANIMATION_START -> disableOverviewMode();
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

    @Override
    public OverviewModeAlphaObserver getOverviewModeAlphaObserver() {
        return alpha -> {
            mOverviewColorAlpha = (float) alpha;
            @ColorInt int color = assumeNonNull(mOverviewColorSupplier.get());
            processOverviewColor(color, mOverviewColorAlpha);
        };
    }

    @VisibleForTesting
    boolean getOverviewMode() {
        return mOverviewMode;
    }

    private void disableOverviewMode() {
        mOverviewMode = false;
        mOverviewColorSupplier.set(Color.TRANSPARENT);
        mColorSchemeUpdate = null;
    }

    private void enableOverviewMode() {
        mOverviewMode = true;
        onFocusedPaneChange(mFocusedPaneSupplier.get());
    }

    private void onFocusedPaneChange(@Nullable Pane focusedPane) {
        @HubColorScheme int newColorScheme = HubColors.getColorSchemeSafe(focusedPane);
        updateColorScheme(newColorScheme);
    }

    private void onHubVisibilityChange(boolean isVisible) {
        processStateChange(isVisible ? HUB_SHOWN : HUB_CLOSED);
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

    private void processOverviewColor(
            @ColorInt int color, @FloatRange(from = 0f, to = 1f) float alpha) {
        if (mOverviewMode) {
            color = ColorUtils.setAlphaComponentWithFloat(color, alpha);
            mOverviewColorSupplier.set(color);
        } else {
            mOverviewColorSupplier.set(Color.TRANSPARENT);
        }
    }
}
