// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.components.module_installer.builder.ModuleInterface;

/** Provides delegate interfaces that can be used to call into VR.  */
@ModuleInterface(module = "vr", impl = "org.chromium.chrome.browser.vr.VrDelegateProviderImpl")
/* package */ interface VrDelegateProvider {
    VrDelegate getDelegate();
    VrIntentDelegate getIntentDelegate();
}
