// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.annotations.UsedByReflection;

/** {@link VrDelegateProvider} implementation if the VR module is available. */
@UsedByReflection("VrModuleProvider.java")
public class VrDelegateProviderImpl implements VrDelegateProvider {
    private final VrDelegateImpl mDelegate = new VrDelegateImpl();
    private final VrIntentDelegateImpl mIntentDelegate = new VrIntentDelegateImpl();

    @UsedByReflection("VrModuleProvider.java")
    public VrDelegateProviderImpl() {}

    @Override
    public VrDelegate getDelegate() {
        return mDelegate;
    }

    @Override
    public VrIntentDelegate getIntentDelegate() {
        return mIntentDelegate;
    }
}
