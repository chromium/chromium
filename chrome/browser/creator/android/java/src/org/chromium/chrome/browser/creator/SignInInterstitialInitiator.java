// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import org.chromium.build.annotations.NullMarked;

/** Interface for showing sign-in insterstitial. */
@NullMarked
public interface SignInInterstitialInitiator {
    /** Shows a sign-in interstitial. */
    void showSignInInterstitial();
}
