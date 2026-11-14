// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Creates a {@link PersistedTabDataStorage}
 *
 * @param <T> {@link PersistedTabDataStorage} being created
 */
@NullMarked
public interface PersistedTabDataStorageFactory<T extends PersistedTabDataStorage> {
    /**
     * @param profile {@link Profile} associated with {@link PersistedTabDataStorage}
     * @return {@link PersistedTabDataStorage}
     */
    T create(Profile profile);
}
