// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Container class to provide the version and the compatibility with Chrome of the installed VrCore.
 */
@JNINamespace("vr")
public class VrCoreInfo {
    /** Represents the version of the installed GVR SDK. */
    public static class GvrVersion {
        public final int majorVersion;
        public final int minorVersion;
        public final int patchVersion;

        public GvrVersion(int majorVersion, int minorVersion, int patchVersion) {
            this.majorVersion = majorVersion;
            this.minorVersion = minorVersion;
            this.patchVersion = patchVersion;
        }
    }

    public final GvrVersion gvrVersion;
    @VrCoreCompatibility
    public final int compatibility;

    public VrCoreInfo(GvrVersion gvrVersion, int compatibility) {
        this.gvrVersion = gvrVersion;
        this.compatibility = compatibility;
    }

    public long makeNativeVrCoreInfo() {
        return (gvrVersion == null)
                ? VrCoreInfoJni.get().init(VrCoreInfo.this, 0, 0, 0, compatibility)
                : VrCoreInfoJni.get().init(VrCoreInfo.this, gvrVersion.majorVersion,
                        gvrVersion.minorVersion, gvrVersion.patchVersion, compatibility);
    }

    @NativeMethods
    interface Natives {
        long init(VrCoreInfo caller, int majorVersion, int minorVersion, int patchVersion,
                int compatibility);
    }
}
