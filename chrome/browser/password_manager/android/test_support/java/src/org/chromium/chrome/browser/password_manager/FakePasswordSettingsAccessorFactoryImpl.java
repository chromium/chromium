// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/**
 * The factory for creating fake {@link PasswordSettingsAccessor} to be used in integration
 * tests.
 */
public class FakePasswordSettingsAccessorFactoryImpl extends PasswordSettingsAccessorFactory {
    /** Returns the fake implementation of {@link PasswordSettingsAccessor} used for tests. */
    @Override
    public PasswordSettingsAccessor createAccessor() {
        return new FakePasswordSettingsAccessor();
    }
}
