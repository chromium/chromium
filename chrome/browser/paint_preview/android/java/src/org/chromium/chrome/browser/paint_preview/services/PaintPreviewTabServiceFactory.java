// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview.services;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * The Java-side implementations of paint_preview_tab_service_factory.cc. Provides an instance of
 * {@link PaintPreviewTabService}.
 */
@NullMarked
@JNINamespace("paint_preview")
public class PaintPreviewTabServiceFactory {
    /**
     * Returns the {@link PaintPreviewTabService} for the given {@link Profile}, or {@code null} if
     * the profile is off-the-record (incognito).
     */
    public static @Nullable PaintPreviewTabService getServiceInstance(Profile profile) {
        return PaintPreviewTabServiceFactoryJni.get().getServiceInstanceForProfile(profile);
    }

    @NativeMethods
    public interface Natives {
        @Nullable PaintPreviewTabService getServiceInstanceForProfile(
                @JniType("Profile*") Profile profile);
    }
}
