// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import androidx.annotation.NonNull;

/**
 * A public API which can be used by other non re-auth clients to get information about the
 * Incognito re-authentication.
 *
 * <p>TODO(crbug.com/40056462): Move this to a separate incognito public_api target and add tests
 * for this public interface.
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
     * A method to add an {@link IncognitoReauthCallback}.
     *
     * @param incognitoReauthCallback {@link IncognitoReauthCallback} that the clients can add to be
     *         notified when the user attempts re-authentication in the Incognito page.
     */
    void addIncognitoReauthCallback(
            @NonNull IncognitoReauthManager.IncognitoReauthCallback incognitoReauthCallback);

    /**
     * A method to remove the {@link IncognitoReauthCallback}.
     *
     * @param incognitoReauthCallback {@link IncognitoReauthCallback} that the clients added to be
     *         notified when the user attempts re-authentication in the Incognito page.
     */
    void removeIncognitoReauthCallback(
            @NonNull IncognitoReauthManager.IncognitoReauthCallback incognitoReauthCallback);

    /**
     * TODO(crbug.com/40056462): This method is ill-placed. Find a better design to restrict
     * non-intended clients to call this method.
     */
    void destroy();
}
