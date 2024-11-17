// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Represents the permission state in service calls. */
@IntDef({PermissionStatus.ALLOW, PermissionStatus.BLOCK, PermissionStatus.ASK})
@Retention(RetentionPolicy.SOURCE)
public @interface PermissionStatus {
    int ALLOW = 0;
    int BLOCK = 1;
    int ASK = 2;
}
