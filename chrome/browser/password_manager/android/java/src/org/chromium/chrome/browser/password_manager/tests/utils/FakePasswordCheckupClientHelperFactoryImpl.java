// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.tests.utils;

import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelperFactory;

/**
 * The factory for creating a fake {@link PasswordCheckupClientHelper} to be used in integration
 * tests.
 */
public class FakePasswordCheckupClientHelperFactoryImpl extends PasswordCheckupClientHelperFactory {
    private PasswordCheckupClientHelper mHelper;

    /**
     * Returns the fake implementation of {@link PasswordCheckupClientHelper} used for tests.
     */
    @Override
    public PasswordCheckupClientHelper createHelper() {
        if (mHelper == null) {
            mHelper = new FakePasswordCheckupClientHelper();
        }
        return mHelper;
    }
}
