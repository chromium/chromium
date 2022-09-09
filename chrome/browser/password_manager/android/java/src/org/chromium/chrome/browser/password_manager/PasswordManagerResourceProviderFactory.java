// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/**
 * Factory for creating {@link PasswordManagerResourceProvider}
 */
public class PasswordManagerResourceProviderFactory {
    /**
     * Creates an instance of PasswordManagerResourceProvider
     * @return {@link PasswordManagerResourceProvider}
     */
    public static PasswordManagerResourceProvider create() {
        return new PasswordManagerResourceProviderImpl();
    }
}
