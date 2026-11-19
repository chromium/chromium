// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface to the Chrome preference storage used to keep the last visibility state of the info
 * header.
 */
@NullMarked
public interface InfoHeaderPref {
    default boolean isVisible() {
        return false;
    }

    default void setVisible(boolean visible) {}
}
