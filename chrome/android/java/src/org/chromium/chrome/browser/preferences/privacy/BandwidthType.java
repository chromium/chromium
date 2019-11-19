// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Bandwidth options available based on network.
 */
public class BandwidthType {
    @IntDef({Type.NEVER_PRERENDER, Type.PRERENDER_ON_WIFI, Type.ALWAYS_PRERENDER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        // Values are numbered from 0 and can't have gaps.
        int NEVER_PRERENDER = 0;
        int PRERENDER_ON_WIFI = 1; // Default option.
        int ALWAYS_PRERENDER = 2;
        int NUM_ENTRIES = 3;
    }

    private final static String[] TITLES = {
            "never_prefetch", "prefetch_on_wifi", "always_prefetch"};

    /**
     * Returns the title of the bandwidthType.
     * @return title
     */
    public static String title(@Type int type) {
        return TITLES[type];
    }

    /**
     * Get the BandwidthType from the title.
     * @param title
     * @return BandwidthType
     */
    public static @BandwidthType.Type int getBandwidthFromTitle(String title) {
        for (@BandwidthType.Type int i = Type.NEVER_PRERENDER; i < Type.NUM_ENTRIES; i++) {
            if (TITLES[i].equals(title)) return i;
        }
        assert false;
        return Type.PRERENDER_ON_WIFI;
    }
}
