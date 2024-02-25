// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.cablev2_authenticator;

import androidx.fragment.app.Fragment;

import org.chromium.components.module_installer.builder.ModuleInterface;

/**
 * The interface to the dynamically installable module for the caBLE v2 authenticator. See {@link
 * ModuleInterface} for the interface to the synthesized class called "Cablev2AuthenticatorModule"
 * that will be generated to manage the module.
 *
 * Use {@link CableAuthenticatorInstaller} to access this module from outside of this package.
 */
@ModuleInterface(
        module = "cablev2_authenticator",
        impl = "org.chromium.chrome.modules.cablev2_authenticator.ModuleImpl")
public interface Module {
    /** Returns a {@link Fragment} that contains the authenticator UI. */
    public Fragment getFragment();
}
