// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.content.Context;

import com.google.ar.core.ArCoreApk;

import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.vr.ArCoreShim.Availability;
import org.chromium.chrome.browser.vr.ArCoreShim.InstallStatus;

@UsedByReflection("ArCoreJavaUtils.java")
class ArCoreShimImpl implements ArCoreShim {
    @UsedByReflection("ArCoreJavaUtils.java")
    public ArCoreShimImpl() {}

    @Override
    public @InstallStatus int requestInstall(Activity activity, boolean userRequestedInstall)
            throws UnavailableDeviceNotCompatibleException,
                   UnavailableUserDeclinedInstallationException {
        try {
            ArCoreApk.InstallStatus installStatus =
                    ArCoreApk.getInstance().requestInstall(activity, userRequestedInstall);
            return mapArCoreApkInstallStatus(installStatus);
        } catch (com.google.ar.core.exceptions.UnavailableDeviceNotCompatibleException e) {
            throw new UnavailableDeviceNotCompatibleException(e);
        } catch (com.google.ar.core.exceptions.UnavailableUserDeclinedInstallationException e) {
            throw new UnavailableUserDeclinedInstallationException(e);
        }
    }

    @Override
    public @Availability int checkAvailability(Context applicationContext) {
        // ARCore's checkAvailability reads shared preferences via ArCoreContentProvider, need to
        // turn off strict mode to allow that.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            ArCoreApk.Availability availability =
                    ArCoreApk.getInstance().checkAvailability(applicationContext);
            return mapArCoreApkAvailability(availability);
        }
    }

    private @InstallStatus int mapArCoreApkInstallStatus(ArCoreApk.InstallStatus installStatus) {
        switch (installStatus) {
            case INSTALLED:
                return InstallStatus.INSTALLED;
            case INSTALL_REQUESTED:
                return InstallStatus.INSTALL_REQUESTED;
            default:
                throw new RuntimeException(
                        String.format("Unknown value of InstallStatus: %s", installStatus));
        }
    }

    private @Availability int mapArCoreApkAvailability(ArCoreApk.Availability availability) {
        switch (availability) {
            case SUPPORTED_APK_TOO_OLD:
                return Availability.SUPPORTED_APK_TOO_OLD;
            case SUPPORTED_INSTALLED:
                return Availability.SUPPORTED_INSTALLED;
            case SUPPORTED_NOT_INSTALLED:
                return Availability.SUPPORTED_NOT_INSTALLED;
            case UNKNOWN_CHECKING:
                return Availability.UNKNOWN_CHECKING;
            case UNKNOWN_ERROR:
                return Availability.UNKNOWN_ERROR;
            case UNKNOWN_TIMED_OUT:
                return Availability.UNKNOWN_TIMED_OUT;
            case UNSUPPORTED_DEVICE_NOT_CAPABLE:
                return Availability.UNSUPPORTED_DEVICE_NOT_CAPABLE;
            default:
                throw new RuntimeException(
                        String.format("Unknown value of Availability: %s", availability));
        }
    }
}
