// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/**
 * This factory returns an implementation for the helper. The factory itself is also implemented
 * downstream.
 */
public abstract class PasswordCheckupClientHelperFactory {
    /**
     * Creates a new instance of PasswordCheckupClientHelperFactory.
     */
    public static PasswordCheckupClientHelperFactory getInstance() {
        return new PasswordCheckupClientHelperFactoryImpl();
    }

    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return An implementation of the {@link PasswordCheckupClientHelper} if one exists.
     */
    public PasswordCheckupClientHelper createHelper() {
        return null;
    }
}
