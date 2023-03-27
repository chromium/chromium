// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;

/**
 * An interface for providing and handling quick-delete operations, such as the browsing data
 * deletion.
 */
public interface QuickDeleteDelegate {
    /**
     * Performs the data deletion for the quick delete feature.
     *
     * @param onDeleteFinished A {@link Runnable} to be called once the browsing data has been
     *         cleared.
     */
    void performQuickDelete(@NonNull Runnable onDeleteFinished);
}
