// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview.services;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * The Java-side implementations of paint_preview_tab_service_factory.cc. Provides an instance of
 * {@link PaintPreviewTabService}.
 */
@JNINamespace("paint_preview")
public class PaintPreviewTabServiceFactory {
    public static PaintPreviewTabService getServiceInstance() {
        return PaintPreviewTabServiceFactoryJni.get().getServiceInstanceForCurrentProfile();
    }

    @NativeMethods
    interface Natives {
        PaintPreviewTabService getServiceInstanceForCurrentProfile();
    }
}
