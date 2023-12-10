// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

/** Set of system conditions to trigger background processing. */
public class TriggerConditions {
    private final boolean mRequirePowerConnected;
    private final int mMinimumBatteryPercentage;
    private final boolean mRequireUnmeteredNetwork;

    /**
     * Creates set of device, network, and power conditions for triggering processing.
     * @param requirePowerConnected whether to require that device is connected to power
     * @param minimumBatteryPercentage minimum percentage (0-100) of remaining battery power
     * @param requireUnmeteredNetwork whether to require connection to unmetered network
     */
    public TriggerConditions(
            boolean requirePowerConnected,
            int minimumBatteryPercentage,
            boolean requireUnmeteredNetwork) {
        mRequirePowerConnected = requirePowerConnected;
        mMinimumBatteryPercentage = minimumBatteryPercentage;
        mRequireUnmeteredNetwork = requireUnmeteredNetwork;
    }

    /** Returns whether connection to power is required. */
    public boolean requirePowerConnected() {
        return mRequirePowerConnected;
    }

    /** Returns the minimum battery percentage that is required. */
    public int getMinimumBatteryPercentage() {
        return mMinimumBatteryPercentage;
    }

    /** Returns whether connection to an unmetered network is required. */
    public boolean requireUnmeteredNetwork() {
        return mRequireUnmeteredNetwork;
    }

    @Override
    public int hashCode() {
        int hash = 13;
        hash = hash * 31 + (mRequirePowerConnected ? 1 : 0);
        hash = hash * 31 + mMinimumBatteryPercentage;
        hash = hash * 31 + (mRequireUnmeteredNetwork ? 1 : 0);
        return hash;
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof TriggerConditions)) return false;
        TriggerConditions otherTriggerConditions = (TriggerConditions) other;
        return mRequirePowerConnected == otherTriggerConditions.mRequirePowerConnected
                && mMinimumBatteryPercentage == otherTriggerConditions.mMinimumBatteryPercentage
                && mRequireUnmeteredNetwork == otherTriggerConditions.mRequireUnmeteredNetwork;
    }
}
