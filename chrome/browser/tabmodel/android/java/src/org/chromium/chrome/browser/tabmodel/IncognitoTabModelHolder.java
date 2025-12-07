// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;

/**
 * Holds an {@link IncognitoTabModelInternal} and an incognito {@link TabGroupModelFilterInternal}.
 */
@NullMarked
/*package*/ class IncognitoTabModelHolder {
    public final IncognitoTabModelInternal tabModel;
    public final TabGroupModelFilterInternal tabGroupModelFilter;

    public IncognitoTabModelHolder(
            IncognitoTabModelInternal tabModel, TabGroupModelFilterInternal tabGroupModelFilter) {
        this.tabModel = tabModel;
        this.tabGroupModelFilter = tabGroupModelFilter;
    }
}
