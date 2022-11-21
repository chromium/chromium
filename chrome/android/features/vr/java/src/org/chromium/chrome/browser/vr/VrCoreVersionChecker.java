// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import androidx.annotation.IntDef;

import com.google.vr.ndk.base.Version;
import com.google.vr.vrcore.base.api.VrCoreNotAvailableException;
import com.google.vr.vrcore.base.api.VrCoreUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class to check if VrCore version is compatible with Chromium.
 */
public class VrCoreVersionChecker {
    @IntDef({VrCoreCompatibility.VR_NOT_SUPPORTED, VrCoreCompatibility.VR_NOT_AVAILABLE,
            VrCoreCompatibility.VR_OUT_OF_DATE, VrCoreCompatibility.VR_READY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface VrCoreCompatibility {
        int VR_NOT_SUPPORTED = 0;
        int VR_NOT_AVAILABLE = 1;
        int VR_OUT_OF_DATE = 2;
        int VR_READY = 3;
    }

    private static final String TAG = "VrCoreVersionChecker";

    public static final String VR_CORE_PACKAGE_ID = "com.google.vr.vrcore";

    public @VrCoreCompatibility int getVrCoreCompatibility() {
        try {
            String vrCoreSdkLibraryVersionString =
                    VrCoreUtils.getVrCoreSdkLibraryVersion(ContextUtils.getApplicationContext());
            Version vrCoreSdkLibraryVersion = Version.parse(vrCoreSdkLibraryVersionString);
            Version targetSdkLibraryVersion =
                    Version.parse(com.google.vr.ndk.base.BuildConstants.VERSION);
            if (!vrCoreSdkLibraryVersion.isAtLeast(targetSdkLibraryVersion)) {
                return VrCoreCompatibility.VR_OUT_OF_DATE;
            }
            return VrCoreCompatibility.VR_READY;
        } catch (VrCoreNotAvailableException e) {
            Log.i(TAG, "Unable to find VrCore.");
            // Old versions of VrCore are not integrated with the sdk library version check and will
            // trigger this exception even though VrCore is installed.
            // Double check package manager to make sure we are not telling user to install
            // when it should just be an update.
            if (PackageUtils.isPackageInstalled(VR_CORE_PACKAGE_ID)) {
                return VrCoreCompatibility.VR_OUT_OF_DATE;
            }
            return VrCoreCompatibility.VR_NOT_AVAILABLE;
        } catch (SecurityException e) {
            Log.e(TAG, "Cannot query VrCore version: " + e.toString());
            return VrCoreCompatibility.VR_NOT_AVAILABLE;
        }
    }
}
