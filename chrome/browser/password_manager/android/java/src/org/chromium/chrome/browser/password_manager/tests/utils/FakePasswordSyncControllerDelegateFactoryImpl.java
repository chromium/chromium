// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.tests.utils;

import org.chromium.chrome.browser.password_manager.PasswordSyncControllerDelegate;
import org.chromium.chrome.browser.password_manager.PasswordSyncControllerDelegateFactory;

/**
 * The factory for creating fake {@link PasswordSyncControllerDelegate} to be used in integration
 * tests.
 */
public class FakePasswordSyncControllerDelegateFactoryImpl
        extends PasswordSyncControllerDelegateFactory {
    private FakePasswordSyncControllerDelegate mPasswordSyncControllerDelegate;

    @Override
    public PasswordSyncControllerDelegate createDelegate() {
        if (mPasswordSyncControllerDelegate == null) {
            mPasswordSyncControllerDelegate = new FakePasswordSyncControllerDelegate();
        }
        return mPasswordSyncControllerDelegate;
    }
}
