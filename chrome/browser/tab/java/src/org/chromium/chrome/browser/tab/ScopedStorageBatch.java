// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.build.annotations.NullMarked;

/**
 * Represents a batch of operations which will be saved to storage on {@link #close()}. Operations
 * must be called on the UI thread.
 *
 * <p>This interface overrides {@link AutoCloseable#close()} to remove the checked {@link
 * Exception}. This allows implementations to be used in try-with-resources blocks without handling
 * exceptions in a catch block.
 */
@FunctionalInterface
@NullMarked
public interface ScopedStorageBatch extends AutoCloseable {
    @Override
    void close();
}
