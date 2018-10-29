// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

/** Utils to help working with android app bundles. */
public class BundleUtils {
    private static final boolean sIsBundle;

    static {
        boolean isBundle;
        try {
            Class.forName("org.chromium.base.BundleCanary");
            isBundle = true;
        } catch (ClassNotFoundException e) {
            isBundle = false;
        }
        sIsBundle = isBundle;
    }

    /* Returns true if the current build is a bundle. */
    public static boolean isBundle() {
        return sIsBundle;
    }
}
