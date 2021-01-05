// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin;

import androidx.fragment.app.Fragment;

import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerCoordinator;

/**
 * This class is used to test {@link AccountPickerDialogFragment}.
 */
class DummyAccountPickerTargetFragment
        extends Fragment implements AccountPickerCoordinator.Listener {
    @Override
    public void onAccountSelected(String accountName, boolean isDefaultAccount) {}

    @Override
    public void addAccount() {}
}
