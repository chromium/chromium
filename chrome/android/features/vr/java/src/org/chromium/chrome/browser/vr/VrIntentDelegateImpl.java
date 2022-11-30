// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;
import android.content.Intent;

import com.google.vr.ndk.base.DaydreamApi;

/**
 * {@link VrIntentDelegate} implementation if the VR module is available.
 */
/* package */ class VrIntentDelegateImpl extends VrIntentDelegate {
    /* package */ static final String VR_FRE_INTENT_EXTRA = "org.chromium.chrome.browser.vr.VR_FRE";

    @Override
    public Intent setupVrFreIntent(Context context, Intent freIntent) {
        Intent intent = new Intent();
        intent.setClassName(context, VrFirstRunActivity.class.getName());
        intent.addCategory(DAYDREAM_CATEGORY);
        intent.putExtra(VR_FRE_INTENT_EXTRA, new Intent(freIntent));
        return intent;
    }

    @Override
    public void removeVrExtras(Intent intent) {
        if (intent == null) return;
        intent.removeCategory(DAYDREAM_CATEGORY);
        assert !isVrIntent(intent);
    }

    @Override
    public Intent setupVrIntent(Intent intent) {
        return DaydreamApi.setupVrIntent(intent);
    }
}
