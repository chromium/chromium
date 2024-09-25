// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.annotation.DrawableRes;

/**
 * Public version of {@link PasswordManagerResourceProviderUpstreamImpl}. Downstream may provide a
 * different implementation.
 */
class PasswordManagerResourceProviderUpstreamImpl implements PasswordManagerResourceProvider {
    @Override
    public @DrawableRes int getPasswordManagerIcon() {
        return R.drawable.ic_vpn_key_blue;
    }
}
