// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.components.webxr.ArCoreJavaUtils;
import org.chromium.components.webxr.ArDelegate;

/**
 * This class provides methods to call into AR. It will be compiled into Chrome
 * only if |enable_arcore| is set at build time.
 */
@UsedByReflection("ArDelegateProvider.java")
public class ArDelegateImpl implements ArDelegate {
    @UsedByReflection("ArDelegateProvider.java")
    public ArDelegateImpl() {}

    @Override
    public boolean onBackPressed() {
        return ArCoreJavaUtils.onBackPressed();
    }
}
