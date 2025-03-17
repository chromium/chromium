// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import androidx.annotation.StringDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

/** Grabs feedback about the default Omnibox. */
@NullMarked
public class OmniboxFeedbackSource implements FeedbackSource {
    @StringDef({Psd.JSO_STATE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface Psd {
        // Captures whether JumpStart Omnibox was enabled and if it engaged when the User returned
        // to Chrome.
        String JSO_STATE = "Omnibox JSO State";
    }

    private static final Map<String, String> sProductSpecificData = new HashMap<>();

    OmniboxFeedbackSource() {}

    /**
     * Register "Product Specific Data" that will be included in user feedback reports. This data is
     * intended to capture non-sensitive runtime or preference-based state information to improve
     * diagnostics and troubleshooting. The entry persists until explicitly removed, or until Chrome
     * is restarted.
     *
     * <ul>
     *   <li>The data must be non-personally identifiable and adhere to privacy guidelines.
     *   <li>Only product-specific, runtime-determined state information or preference-based
     *       settings are permitted.
     * </ul>
     *
     * Example valid use cases:
     *
     * <ul>
     *   <li>Runtime-Computed Feature States, e.g. features tareting low-end devices.
     *   <li>Preference-Based Settings, e.g. toolbar placement.
     * </ul>
     *
     * @param key Specific product data key to include - {@see Psd}.
     * @param value Value to associate with the key, or null, to remove the entry from the report.
     */
    public static void addFeedbackData(@Psd String key, @Nullable String value) {
        if (value != null) {
            sProductSpecificData.put(key, value);
        } else {
            sProductSpecificData.remove(key);
        }
    }

    @Override
    public Map<String, String> getFeedback() {
        // FeedbackCollector builds its own map, so no need to build a copy here.
        return sProductSpecificData;
    }
}
