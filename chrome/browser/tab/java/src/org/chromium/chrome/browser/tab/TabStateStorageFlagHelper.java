// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Helper class to return values associated with flags for tab state storage functionality. */
@NullMarked
public final class TabStateStorageFlagHelper {
    private TabStateStorageFlagHelper() {}

    /** Returns whether tab state storage functionality is enabled. */
    public static boolean isTabStorageEnabled() {
        return ChromeFeatureList.sTabStorageSqlitePrototype.isEnabled()
                && ChromeFeatureList.sTabCollectionAndroid.isEnabled();
    }

    /** Returns whether tab state storage functionality is authoritative as the source of truth. */
    public static boolean isStorageAuthoritative() {
        return ChromeFeatureList.sTabCollectionAndroid.isEnabled()
                && ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue();
    }
}
