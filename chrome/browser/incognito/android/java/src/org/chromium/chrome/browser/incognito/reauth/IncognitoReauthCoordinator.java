// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import org.chromium.ui.modaldialog.DialogDismissalCause;

/**
 * An interface that provides the fundamental internal API for incognito re-authentication.
 *
 * <p>The derived classes must ensure that they are created and destroyed, each time the incognito
 * re-auth screen is shown/hidden respectively. This allows to release any un-used resource when the
 * re-auth is not shown.
 *
 * <p>TODO(crbug.com/40056462): This and any other internal re-auth related files should be put in
 * an internal folder. Ideally only the controller would be potentially exposed.
 */
interface IncognitoReauthCoordinator {
    /** A method responsible to fire the re-auth screen. */
    void show();

    /**
     * A method responsible to hide the re-auth screen.
     *
     * <p>TODO(crbug.com/40056462): Refactor this since not all the clients who implement this
     * interface are dialog based.
     *
     * @param dismissalCause The {@link DialogDismissalCause} for the dismissal of the re-auth
     *     screen.
     */
    void hide(@DialogDismissalCause int dismissalCause);

    /** A method responsible to do any clean-up when the coordinator is being destroyed. */
    void destroy();
}
