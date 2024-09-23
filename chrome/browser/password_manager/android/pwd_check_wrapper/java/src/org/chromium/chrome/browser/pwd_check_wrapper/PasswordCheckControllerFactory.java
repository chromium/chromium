// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;

public class PasswordCheckControllerFactory {
    public PasswordCheckController create(
            SyncService syncService,
            PrefService prefService,
            PasswordStoreBridge passwordStoreBridge,
            PasswordManagerHelper passwordManagerHelper) {
        if (passwordManagerHelper.canUseUpm()
                || PasswordManagerUtilBridge.isGmsCoreUpdateRequired(prefService, syncService)) {
            return new GmsCorePasswordCheckController(
                    syncService, prefService, passwordStoreBridge, passwordManagerHelper);
        }
        return new ChromeNativePasswordCheckController();
    }
}
