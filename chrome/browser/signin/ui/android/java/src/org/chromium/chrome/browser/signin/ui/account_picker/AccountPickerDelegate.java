// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import org.chromium.base.Callback;
import org.chromium.components.signin.base.GoogleServiceAuthError;

/**
 * This interface is used in web sign-in flow for the account picker bottom sheet.
 * TODO(crbug.com/1219434): Nest this in the coordinator.
 */
public interface AccountPickerDelegate {
    /** Releases resources used by this class. */
    void destroy();

    /** Signs in the user with the given account. */
    void signIn(String accountEmail, Callback<GoogleServiceAuthError> onSignInErrorCallback);
}
