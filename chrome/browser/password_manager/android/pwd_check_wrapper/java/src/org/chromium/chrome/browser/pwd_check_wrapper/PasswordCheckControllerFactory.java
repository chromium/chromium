// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.components.sync.SyncService;

@NullMarked
public class PasswordCheckControllerFactory {
    public PasswordCheckController create(
            @Nullable SyncService syncService,
            PasswordStoreBridge passwordStoreBridge,
            PasswordManagerHelper passwordManagerHelper) {
        // This is only used by the old Safety Check, which is only opened from the PhishGuard
        // dialog and only if the phished credential is saved in both local and account stores.
        // This means that UPM is completely available.
        assert PasswordManagerUtilBridge.isPasswordManagerAvailable();
        return new GmsCorePasswordCheckController(
                syncService, passwordStoreBridge, passwordManagerHelper);
    }
}
