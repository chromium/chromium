// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.cablev2_authenticator;

import androidx.fragment.app.Fragment;

import org.chromium.chrome.browser.webauth.authenticator.CableAuthenticatorUI;

/**
 * This is the implementation of the interface to the caBLE v2 Authenticator module. It should
 * never be accessed directly, only by the module infrastructure machinery. (It's required to be
 * public for that machinery to function.)
 */
public class ModuleImpl implements Module {
    @Override
    public Fragment getFragment() {
        return new CableAuthenticatorUI();
    }
}
