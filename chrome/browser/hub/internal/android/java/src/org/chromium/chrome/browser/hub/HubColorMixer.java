// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.HUB_CLOSED;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.HUB_SHOWN;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.TABLET_ANIMATION_COMPLETE;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.TABLET_ANIMATION_START;

import android.animation.AnimatorSet;
import android.content.Context;
import android.graphics.Color;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Manages and updates the color scheme of the Hub UI.
 *
 * <p>This class observes the Hub's visibility state and the currently focused {@link Pane} to
 * determine the appropriate color palette. It supports different color schemes based on the focused
 * pane and also manages a specific "overview mode" color applied when the Hub is shown (on
 * non-tablet devices) or during the tablet animation.
 *
 * <p>Clients should register their {@link HubViewColorBlend} with this class using {@link
 * #registerBlend(HubViewColorBlend)} to receive color scheme updates. The {@code HubColorMixer}
 * should be passed via the {@link #COLOR_MIXER} property on these models whenever these {@link
 * HubViewColorBlend}s need to be registered.
 */
@NullMarked
public class HubColorMixer {

    /**
     * For phones we follow this sequence of transitions for state changes on Hub open and close:
     *
     * <ul>
     *   <li>HUB_CLOSED -> HUB_SHOWN
     * </ul>
     *
     * For tablets we follow this sequence of transitions for state changes on Hub open and close:
     *
     * <ul>
     *   <li>HUB_CLOSED -> HUB_SHOWN
     *   <li>HUB_SHOWN -> TABLET_ANIMATION_START
     *   <li>TABLET_ANIMATION_START -> TABLET_ANIMATION_COMPLETE
     * </ul>
     */
    @IntDef({HUB_SHOWN, HUB_CLOSED, TABLET_ANIMATION_START, TABLET_ANIMATION_COMPLETE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StateChange {
        int HUB_SHOWN = 0;
        int HUB_CLOSED = 1;
        int TABLET_ANIMATION_START = 2;
        int TABLET_ANIMATION_COMPLETE = 3;
    }

    /** Property key to allow for registering color schemes. */
    public static final WritableObjectPropertyKey<HubColorMixer> COLOR_MIXER =
            new WritableObjectPropertyKey<>();

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
    public HubColorMixer(
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

    /** Cleans up observers. */
    public void destroy() {
        mHubVisibilitySupplier.removeObserver(mOnHubVisibilityObserver);
        mFocusedPaneSupplier.removeObserver(mOnFocusedPaneObserver);
    }

    /**
     * Supplies the current overview mode color. This will be null if overview mode is not enabled.
     */
    public ObservableSupplierImpl<@Nullable Integer> getOverviewColorSupplier() {
        return mOverviewColorSupplier;
    }

    /**
     * Updates overview mode based on the provided reason for the state change.
     *
     * @param colorChangeReason The reason for changing state.
     */
    public void processStateChange(@StateChange int colorChangeReason) {
        switch (colorChangeReason) {
            case HUB_SHOWN, TABLET_ANIMATION_COMPLETE -> enableOverviewMode();
            case TABLET_ANIMATION_START, HUB_CLOSED -> disableOverviewMode();
            default -> throw new IllegalStateException("Unexpected value: " + colorChangeReason);
        }
    }

    /** Registers a {@link HubViewColorBlend} to receive color scheme updates. */
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
