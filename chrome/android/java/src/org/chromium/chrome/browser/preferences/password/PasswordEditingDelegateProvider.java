// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

/**
 * A provider for PasswordEditingDelegate implementations, handling the choice of the proper
 * one (production vs. testing).
 *
 * This class is used by the code responsible for Chrome's passwords settings. The
 * provider is a singleton because it needs to provide a bridge to the password editing
 * UI. The editing UI can't get the bridge any other way because it's an object and
 * it can't be sent to the editing UI through bundle arguments when the UI is started.
 */
public class PasswordEditingDelegateProvider {
    private static final PasswordEditingDelegateProvider INSTANCE =
            new PasswordEditingDelegateProvider();

    private PasswordEditingDelegate mPasswordEditingDelegate;

    /** Private constructor, use GetInstance() instead. */
    private PasswordEditingDelegateProvider() {}

    public static PasswordEditingDelegateProvider getInstance() {
        return INSTANCE;
    }

    /**
     * Sets an implementation of PasswordEditingDelegate to be used.
     */
    public void setPasswordEditingDelegate(PasswordEditingDelegate passwordEditingDelegate) {
        mPasswordEditingDelegate = passwordEditingDelegate;
    }

    /**
     * A function to get a PasswordEditingDelegate implementation.
     */
    public PasswordEditingDelegate getPasswordEditingDelegate() {
        return mPasswordEditingDelegate;
    }
}
