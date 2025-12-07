// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import org.chromium.build.annotations.NullMarked;

/** This interface is responsible for handling UI logic for the safety check fragment's view. */
@NullMarked
public interface SafetyCheckComponentUi {
    /**
     * Used to figure out whether the UI related to the account storage password check should be
     * displayed.
     *
     * @return true if account password storage is used.
     */
    boolean isAccountPasswordStorageUsed();
}
