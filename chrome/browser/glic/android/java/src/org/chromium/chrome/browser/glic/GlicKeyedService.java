// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

@JNINamespace("glic")
@NullMarked
public class GlicKeyedService {

    public GlicKeyedService() {}

    public void toggleUI(long browserWindowPtr, Profile profile, int invocationSource) {
        GlicKeyedServiceJni.get().toggleUI(browserWindowPtr, profile, invocationSource);
    }

    @NativeMethods
    public interface Natives {
        void toggleUI(long browserWindowPtr, @JniType("Profile*") Profile profile, int source);
    }
}
