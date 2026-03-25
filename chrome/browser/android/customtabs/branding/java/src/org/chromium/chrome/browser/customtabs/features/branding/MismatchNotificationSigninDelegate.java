// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;

/** Interface bridging the mismatch notification controller with the sign-in logic. */
@NullMarked
public interface MismatchNotificationSigninDelegate {
    /**
     * Starts the sign-in flow.
     *
     * @param config The sign-in configuration.
     */
    void startSignin(BottomSheetSigninAndHistorySyncConfig config);
}
