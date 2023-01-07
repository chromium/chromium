// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/**
 * The factory for creating a fake {@link CredentialManagerLauncher} to be used in integration
 * tests.
 */
public class FakeCredentialManagerLauncherFactoryImpl extends CredentialManagerLauncherFactory {
    private CredentialManagerLauncher mLauncher;

    /**
     * Returns the fake implementation of {@link CredentialManagerLauncher} used for tests.
     */
    @Override
    public CredentialManagerLauncher createLauncher() {
        if (mLauncher == null) {
            mLauncher = new FakeCredentialManagerLauncher();
        }
        return mLauncher;
    }
}
