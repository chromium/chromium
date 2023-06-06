// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mandatory_reauth;

/**
 * This component allows showing the Mandatory Reauth opt-in prompt in a bottom sheet.
 */
interface MandatoryReauthOptInBottomSheetComponent {
    /**
     * Shows the bottom sheet.
     */
    boolean show();

    /**
     * Closes the bottom sheet.
     */
    void close();
}
