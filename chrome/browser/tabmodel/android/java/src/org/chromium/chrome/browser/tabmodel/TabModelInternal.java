// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

/** Package private internal methods for {@link TabModel}. */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public interface TabModelInternal extends TabModel {
    /**
     * Set when tab model become active and inactive.
     *
     * @param active Whether the tab model is active.
     */
    /* package */ void setActive(boolean active);

    /**
     * To be called when this model should be destroyed. The model should no longer be used after
     * this.
     *
     * <p>As a result of this call, all {@link Tab}s owned by this model should be destroyed.
     */
    /* package */ void destroy();
}
