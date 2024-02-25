// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Helper class to apply multiple features/flags to the global CommandLine singleton. This class and
 * its static methods assume the CommandLine has already been initialized.
 */
public class FlagOverrideHelper {
    private Map<String, Flag> mFlagMap = new HashMap<>();

    public FlagOverrideHelper(Flag[] flagList) {
        for (Flag flag : flagList) {
            mFlagMap.put(flag.getName(), flag);
        }
    }

    @VisibleForTesting
    public static List<String> getCommaDelimitedSwitchValue(String name) {
        return CommandLine.getInstance().hasSwitch(name)
                ? Arrays.asList(CommandLine.getInstance().getSwitchValue(name).split(","))
                : new ArrayList<>();
    }

    @VisibleForTesting
    public static void setCommaDelimitedSwitchValue(String name, @NonNull List<String> value) {
        if (value.isEmpty()) {
            CommandLine.getInstance().removeSwitch(name);
        } else {
            CommandLine.getInstance().appendSwitchWithValue(name, TextUtils.join(",", value));
        }
    }

    /**
     * Apply the flag overrides specified in {@code overrides} to the {@link CommandLine} singleton.
     * The flag names in {@code overrides} must have been already declared in the {@code flagList}
     * passed to this instance's constructor.
     *
     * @param overrides a Map of flag names to override state (enabled or disabled).
     */
    public void applyFlagOverrides(Map<String, Boolean> overrides) {
        Set<String> enabledFeatures = new HashSet<>();
        Set<String> disabledFeatures = new HashSet<>();
        enabledFeatures.addAll(getCommaDelimitedSwitchValue("enable-features"));
        disabledFeatures.addAll(getCommaDelimitedSwitchValue("disable-features"));

        for (Map.Entry<String, Boolean> entry : overrides.entrySet()) {
            Flag flag = getFlagForName(entry.getKey());
            boolean enabled = entry.getValue();
            if (flag.isBaseFeature()) {
                if (enabled) {
                    enabledFeatures.add(flag.getName());
                    disabledFeatures.remove(flag.getName());
                } else {
                    enabledFeatures.remove(flag.getName());
                    disabledFeatures.add(flag.getName());
                }
            } else {
                if (enabled && flag.getEnabledStateValue() != null) {
                    CommandLine.getInstance()
                            .appendSwitchWithValue(flag.getName(), flag.getEnabledStateValue());
                } else if (enabled) {
                    CommandLine.getInstance().appendSwitch(flag.getName());
                } else {
                    CommandLine.getInstance().removeSwitch(flag.getName());
                }
            }
        }

        setCommaDelimitedSwitchValue("enable-features", new ArrayList<>(enabledFeatures));
        setCommaDelimitedSwitchValue("disable-features", new ArrayList<>(disabledFeatures));
    }

    /**
     * Fetches a {@link Flag} corresponding to {@code name}, based on the Flag array passed to the
     * constructor.
     *
     * @param name the name of the {@link Flag} to look up.
     * @return the desired {@link Flag}.
     * @throws RuntimeException if this cannot find {@code name} in the list.
     */
    public Flag getFlagForName(@NonNull String name) {
        if (mFlagMap.containsKey(name)) {
            return mFlagMap.get(name);
        }
        // This should not be reached.
        throw new RuntimeException("Unable to find flag '" + name + "' in the list.");
    }
}
