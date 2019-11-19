// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import androidx.annotation.Nullable;

/**
 * Java counterpart to C++ ContentSetting enum.
 *
 * @see ContentSettingValues
 *
 * Note 1: We assume, that ContentSettingValues are numbered from 0 and don't have gaps.
 * Note 2: All updates for ContentSettingValues (including order of entries) must also
 *         be reflected in {@link STRING_VALUES}.
 */
public class ContentSetting {
    // Indexed by {@link ContentSettingValues}.
    private final static String[] STRING_VALUES = {
            "DEFAULT", // ContentSettingValues.DEFAULT
            "ALLOW", // ContentSettingValues.ALLOW
            "BLOCK", // ContentSettingValues.BLOCK
            "ASK", // ContentSettingValues.ASK
            "SESSION_ONLY", // ContentSettingValues.SESSION_ONLY
            "DETECT_IMPORTANT_CONTENT", // ContentSettingValues.DETECT_IMPORTANT_CONTENT
    };

    public static String toString(@ContentSettingValues int value) {
        assert ContentSettingValues.DEFAULT == 0;
        assert ContentSettingValues.NUM_SETTINGS == STRING_VALUES.length;
        return STRING_VALUES[value];
    }

    /**
     * Converts a string to its equivalent #Value.
     * @param value The string to convert.
     * @return What value the enum is representing (or null if failed).
     */
    public static @Nullable @ContentSettingValues Integer fromString(String value) {
        assert ContentSettingValues.DEFAULT == 0;
        assert ContentSettingValues.NUM_SETTINGS == STRING_VALUES.length;

        for (int i = 0; i < ContentSettingValues.NUM_SETTINGS; ++i) {
            if (STRING_VALUES[i].equals(value)) return i;
        }
        return null;
    }
}
