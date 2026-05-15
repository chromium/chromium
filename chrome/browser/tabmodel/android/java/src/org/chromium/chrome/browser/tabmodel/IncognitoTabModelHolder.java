// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;

/** Holds an {@link IncognitoTabModelInternal}. */
@NullMarked
/*package*/ class IncognitoTabModelHolder {
    public final IncognitoTabModelInternal tabModel;

    public IncognitoTabModelHolder(IncognitoTabModelInternal tabModel) {
        this.tabModel = tabModel;
    }
}
