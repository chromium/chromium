// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview.services;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

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
