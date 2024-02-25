// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.webxr.VrCompositorDelegate;
import org.chromium.components.webxr.VrCompositorDelegateProvider;
import org.chromium.content_public.browser.WebContents;

/** Concrete, Chrome-specific implementation of VrCompositorDelegateProvider interface. */
@JNINamespace("vr")
public class VrCompositorDelegateProviderImpl implements VrCompositorDelegateProvider {
    @CalledByNative
    public VrCompositorDelegateProviderImpl() {}

    @Override
    public VrCompositorDelegate create(WebContents webContents) {
        return new VrCompositorDelegateImpl(webContents);
    }
}
