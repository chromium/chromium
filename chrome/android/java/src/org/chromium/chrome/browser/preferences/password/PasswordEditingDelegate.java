// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

/**
 * Interface for accessing the PasswordEditingBridge. It's used to mock it in tests.
 */
public interface PasswordEditingDelegate {
    /**
     * Edits the credential record that was loaded in the PasswordEntryEditor.
     *
     * @param newUsername The new username value.
     * @param newPassword The new password value.
     */
    void editSavedPasswordEntry(String newUsername, String newPassword);

    /**
     * Destroy the native object. This needs to be called after using the bridge is done.
     */
    void destroy();
}
