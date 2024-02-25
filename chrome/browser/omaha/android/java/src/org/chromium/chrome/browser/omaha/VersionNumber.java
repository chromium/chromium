// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import java.util.Locale;

/** Utility for dealing with Chrome version numbers. */
public class VersionNumber {
    private final int[] mVersion = {0, 0, 0, 0};

    /**
     * Parses out the version numbers from a given version string.
     * @param str a version number of the format a.b.c.d, where each is an integer.
     * @return A VersionNumber containing the version info, or null if it couldn't be parsed.
     */
    public static VersionNumber fromString(String str) {
        if (str == null) {
            return null;
        }

        // Parse out the version numbers.
        String[] pieces = str.split("\\.");
        if (pieces.length != 4) {
            return null;
        }

        VersionNumber version = new VersionNumber();
        try {
            for (int i = 0; i < 4; ++i) {
                version.mVersion[i] = Integer.parseInt(pieces[i]);
            }
        } catch (NumberFormatException e) {
            return null;
        }

        return version;
    }

    @Override
    public String toString() {
        return String.format(
                Locale.US, "%d.%d.%d.%d", mVersion[0], mVersion[1], mVersion[2], mVersion[3]);
    }

    /**
     * @return whether this VersionNumber is smaller than the given one, going from left to right.
     */
    public boolean isSmallerThan(VersionNumber version) {
        for (int i = 0; i < 4; ++i) {
            if (mVersion[i] < version.mVersion[i]) {
                return true;
            } else if (mVersion[i] > version.mVersion[i]) {
                return false;
            }
        }
        return false;
    }
}
