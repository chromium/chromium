// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;

/**
 * Manages the Contextual Search disable-able promo tap counter for privacy opt-in/out.
 * This counter stores a single persistent integer preference that can indicate both a count
 * and whether it's been disabled (and remembers the count before being disabled).
 * TODO(donnd): remove this class and its usage since the value is no longer clear.
 */
class DisableablePromoTapCounter {

    // --------------------------------------------------------------------------------------------
    // Opt-out style Promo counter
    //
    // The Opt-out style promo tap counter needs to do two things:
    // 1) Count Taps that trigger the promo, so they can be limited.
    // 2) Support a "disabled" state; when the user opens the panel then Taps trigger from then on.
    // We use a single persistent setting to record both meanings by using a negative value to
    // indicate disabled.
    // --------------------------------------------------------------------------------------------

    // Amount to bias a disabled value when making it negative (so 0 can be disabled).
    private static final int PROMO_TAPS_DISABLED_BIAS = -1;

    private static DisableablePromoTapCounter sInstance;

    private final SharedPreferencesManager mPrefsManager;
    private int mCounter;

    /**
     * Gets the singleton instance used to access the persistent counter.
     * @param prefsManager The ChromePreferenceManager to get prefs from.
     * @return the counter.
     */
    static DisableablePromoTapCounter getInstance(SharedPreferencesManager prefsManager) {
        if (sInstance == null) {
            sInstance = new DisableablePromoTapCounter(prefsManager);
        }
        return sInstance;
    }

    /**
     * Private constructor -- use {@link #getInstance} to get the singleton instance.
     * @param prefsManager The preferences manager to use.
     */
    private DisableablePromoTapCounter(SharedPreferencesManager prefsManager) {
        mPrefsManager = prefsManager;
        setRawCounter(
                prefsManager.readInt(
                        ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT));
    }

    /**
     * @return whether the counter is currently enabled.
     */
    boolean isEnabled() {
        return mCounter >= 0;
    }

    /** Disables the counter. */
    void disable() {
        if (isEnabled()) setRawCounter(getToggledCounter(mCounter));
    }

    /**
     * @return The current count (always non-negative).
     */
    int getCount() {
        if (isEnabled()) return mCounter;

        return getToggledCounter(mCounter);
    }

    /** Increments the counter. */
    void increment() {
        assert isEnabled();
        setRawCounter(getCount() + 1);
    }

    /** Resets the counter to zero and enabled. */
    @VisibleForTesting
    void reset() {
        setRawCounter(0);
    }

    /**
     * Sets the persistent storage to the given value.
     * @param rawCounter The raw value to write.
     */
    private void setRawCounter(int rawCounter) {
        mCounter = rawCounter;
        writeRawCounter();
    }

    /** Writes the current counter's raw value to persistent storage. */
    private void writeRawCounter() {
        mPrefsManager.writeInt(
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT, mCounter);
    }

    /**
     * Toggles the counter's raw value from the enabled to disabled state, or vice versa.
     * @param rawCounter The current raw counter value.
     * @return The toggled raw counter value.
     */
    private int getToggledCounter(int rawCounter) {
        // In order to encode a 0 value we need to introduce a bias when we create negative
        // raw values.  Since -1 is a special value for some code, we make the bias big
        // enough that we'll never have a raw value of -1 (though that's not strictly needed).
        return PROMO_TAPS_DISABLED_BIAS - rawCounter;
    }
}
