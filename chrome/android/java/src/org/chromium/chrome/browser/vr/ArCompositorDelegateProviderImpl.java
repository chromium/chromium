// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.webxr.ArCompositorDelegate;
import org.chromium.components.webxr.ArCompositorDelegateProvider;
import org.chromium.content_public.browser.WebContents;

/** Concrete, Chrome-specific implementation of ArCompositorDelegateProvider interface. */
@JNINamespace("vr")
public class ArCompositorDelegateProviderImpl implements ArCompositorDelegateProvider {
    @CalledByNative
    public ArCompositorDelegateProviderImpl() {}

    @Override
    public ArCompositorDelegate create(WebContents webContents) {
        return new ArCompositorDelegateImpl(webContents);
    }
}
