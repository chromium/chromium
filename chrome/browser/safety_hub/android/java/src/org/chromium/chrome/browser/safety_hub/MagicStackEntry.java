// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.StringDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Data container class that contains the module type and its description. */
class MagicStackEntry {
    @Retention(RetentionPolicy.SOURCE)
    @StringDef({
        ModuleType.SAFE_BROWSING,
        ModuleType.NOTIFICATION_PERMISSIONS,
        ModuleType.REVOKED_PERMISSIONS,
        ModuleType.PASSWORDS
    })
    @interface ModuleType {
        String SAFE_BROWSING = "safe_browsing";
        String NOTIFICATION_PERMISSIONS = "notification_permissions";
        String REVOKED_PERMISSIONS = "revoked_permissions";
        String PASSWORDS = "passwords";
    }

    private final String mDescription;
    private final @ModuleType String mModuleType;

    private MagicStackEntry(String description, @ModuleType String moduleType) {
        mDescription = description;
        mModuleType = moduleType;
    }

    @CalledByNative
    static MagicStackEntry create(
            @JniType("std::u16string") String description,
            @JniType("std::string") String moduleType) {
        return new MagicStackEntry(description, moduleType);
    }

    String getDescription() {
        return mDescription;
    }

    @ModuleType
    String getModuleType() {
        return mModuleType;
    }
}
