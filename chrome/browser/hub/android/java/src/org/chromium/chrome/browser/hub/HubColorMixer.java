// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.HUB_CLOSED;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.HUB_SHOWN;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.TRANSLATE_DOWN_TABLET_ANIMATION_START;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.TRANSLATE_UP_TABLET_ANIMATION_END;

import androidx.annotation.IntDef;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;
import java.util.function.DoubleConsumer;

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
public interface HubColorMixer {

    /**
     * For phones we follow this sequence of transitions for state changes on Hub open and close:
     *
     * <ul>
     *   <li>HUB_CLOSED -> HUB_SHOWN
     *   <li>HUB_SHOWN -> HUB_CLOSED
     * </ul>
     *
     * <p>For tablets we follow this sequence of transitions for state changes on Hub open:
     *
     * <ul>
     *   <li>HUB_CLOSED -> HUB_SHOWN
     *   <li>HUB_SHOWN -> TRANSLATE_UP_TABLET_ANIMATION_END
     * </ul>
     *
     * <p>For tablets we follow this sequence of transitions for state changes on Hub close:
     *
     * <ul>
     *   <li>HUB_SHOWN -> HUB_CLOSED
     *   <li>HUB_CLOSED -> TRANSLATE_DOWN_TABLET_ANIMATION_START
     * </ul>
     */
    @IntDef({
        HUB_SHOWN,
        HUB_CLOSED,
        TRANSLATE_UP_TABLET_ANIMATION_END,
        TRANSLATE_DOWN_TABLET_ANIMATION_START,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface StateChange {
        int HUB_SHOWN = 0;
        int HUB_CLOSED = 1;
        int TRANSLATE_UP_TABLET_ANIMATION_END = 2;
        int TRANSLATE_DOWN_TABLET_ANIMATION_START = 3;
    }

    /**
     * Observes alpha of the overview during a fade animation. The partially transparent overview is
     * drawn over top of the toolbar during this time.
     */
    @FunctionalInterface
    interface OverviewModeAlphaObserver extends DoubleConsumer {}

    /** Property key to allow for registering color schemes. */
    PropertyModel.WritableObjectPropertyKey<HubColorMixer> COLOR_MIXER =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** Cleans up observers. */
    void destroy();

    /**
     * Supplies the current overview mode color. This will be Color.TRANSPARENT if overview mode is
     * not enabled.
     */
    NonNullObservableSupplier<Integer> getOverviewColorSupplier();

    /**
     * Updates overview mode based on the provided reason for the state change.
     *
     * @param colorChangeReason The reason for changing state.
     */
    void processStateChange(@StateChange int colorChangeReason);

    /** Registers a {@link HubViewColorBlend} to receive color scheme updates. */
    void registerBlend(HubViewColorBlend colorBlend);

    /** Unregisters a {@link HubViewColorBlend} to cease receiving color scheme updates. */
    void unregisterBlend(HubViewColorBlend colorBlend);

    /** Gets the observer for overview mode alpha changes. */
    OverviewModeAlphaObserver getOverviewModeAlphaObserver();

    /** Data object representing an active color scheme blend transition. */
    class ColorBlendProgress {
        public final @HubColorScheme int startScheme;
        public final @HubColorScheme int endScheme;
        public final float fraction;

        public ColorBlendProgress(
                @HubColorScheme int startScheme, @HubColorScheme int endScheme, float fraction) {
            this.startScheme = startScheme;
            this.endScheme = endScheme;
            this.fraction = fraction;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o instanceof ColorBlendProgress that) {
                return startScheme == that.startScheme
                        && endScheme == that.endScheme
                        && Float.compare(fraction, that.fraction) == 0;
            }
            return false;
        }

        @Override
        public int hashCode() {
            return Objects.hash(startScheme, endScheme, fraction);
        }
    }
}
