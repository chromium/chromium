// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.ui.vr.VrModeObserver;
import org.chromium.ui.vr.VrModeProvider;

/**
 * A simple implementation of VrModeProvider that passes calls to VrModuleProvider.
 */
public class VrModeProviderImpl implements VrModeProvider {
    @Override
    public boolean isInVr() {
        return VrModuleProvider.getDelegate().isInVr();
    }

    @Override
    public void registerVrModeObserver(VrModeObserver observer) {
        VrModuleProvider.registerVrModeObserver(observer);
    }

    @Override
    public void unregisterVrModeObserver(VrModeObserver observer) {
        VrModuleProvider.unregisterVrModeObserver(observer);
    }
}
