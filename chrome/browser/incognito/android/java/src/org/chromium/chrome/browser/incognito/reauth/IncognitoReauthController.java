// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

/**
 * A public API which can be used by other non re-auth clients to get information about the
 * Incognito re-authentication.
 *
 * TODO(crbug.com/1227656): Move this to a separate incognito public_api target and add tests for
 * this public interface.
 */
public interface IncognitoReauthController {
    /**
     * @return True if the Incognito re-auth page is currently being shown, false otherwise.
     */
    boolean isReauthPageShowing();
    /**
     * @return True if the Incognito re-auth is pending, false otherwise.
     */
    boolean isIncognitoReauthPending();

    /**
     * TODO(crbug.com/1227656): This method is ill-placed. Find a better design to restrict
     * non-intended clients to call this method.
     */
    void destroy();
}
