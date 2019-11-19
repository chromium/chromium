// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

/**
 * Interface used by {@link VrConsentDialog} to notify user consent to a session
 * permission dialog.
 */
public interface VrConsentListener {
    /**
     * Needs to be called in response to user's action to notify the listeners
     * about their decision.
     **/
    public void onUserConsent(boolean allowed);
}
