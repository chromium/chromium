// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;
import android.content.Intent;

/**
 * Fallback {@link VrIntentDelegate} implementation if the VR module is not available.
 */
public class VrIntentDelegateFallback extends VrIntentDelegate {
    @Override
    public Intent setupVrFreIntent(Context context, Intent freIntent) {
        if (VrModuleProvider.getDelegate().bootsToVr()) return freIntent;
        // Don't bother handling FRE without VR module on smartphone VR. Just request module and
        // return to caller.
        VrModuleProvider.installModule((success) -> {});
        VrFallbackUtils.showFailureNotification(context);
        return null;
    }

    @Override
    public void removeVrExtras(Intent intent) {
        assert false;
    }

    @Override
    public Intent setupVrIntent(Intent intent) {
        assert false;
        return intent;
    }
}
