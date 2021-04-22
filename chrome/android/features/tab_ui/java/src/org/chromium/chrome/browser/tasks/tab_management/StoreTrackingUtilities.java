// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

/**
 * A class to handle whether store hours feature is enabled.
 */
public class StoreTrackingUtilities {
    /**
     * @return Whether the show store hours on tabs feature is enabled.
     */
    public static boolean isStoreHoursOnTabsEnabled() {
        // TODO(crbug.com/1198277) This should be enabled when the feature flag is on and the user
        // is signed in
        return false;
    }
}
