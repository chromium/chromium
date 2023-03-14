// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.chrome.browser.modules.ModuleInstallUi;

/**
 * Dummy implementation of VrModuleProvider.
 */
public class VrModuleProvider implements ModuleInstallUi.FailureUiListener {
    private static VrDelegateProvider sDelegateProvider;

    public static void maybeInit() {
        return;
    }

    public static void maybeRequestModuleIfDaydreamReady() {
        return;
    }

    public static VrDelegate getDelegate() {
        return getDelegateProvider().getDelegate();
    }

    private static VrDelegateProvider getDelegateProvider() {
        if (sDelegateProvider == null) {
            sDelegateProvider = new VrDelegateProviderFallback();
        }
        return sDelegateProvider;
    }

    @Override
    public void onFailureUiResponse(boolean retry) {
        return;
    }
}
