// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_INSTALL_FROM_DMG_H_
#define CHROME_BROWSER_MAC_INSTALL_FROM_DMG_H_

#include <string>

enum DiskImageStatus {
  DiskImageStatusFailure,
  DiskImageStatusFalse,
  DiskImageStatusTrue
};

// Returns info about the application is located on a read-only filesystem of a
// disk image. Returns false if not, or in the event of an error. If
// dmg_bsd_device_name is present, it will be set to the BSD device name for
// the disk image's device, in "diskNsM" form.
DiskImageStatus IsAppRunningFromReadOnlyDiskImage(
    std::string* dmg_bsd_device_name);

// If the application is running from a read-only disk image, prompts the user
// to install it to the hard drive.  If the user approves, the application
// will be installed and launched, and MaybeInstallFromDiskImage will return
// true.  In that case, the caller must exit expeditiously.
bool MaybeInstallFromDiskImage();

// Given a BSD device name of the form "diskN" or "diskNsM" as used by IOKit,
// where the device name corresponds to a disk image, unmounts all filesystems
// on that disk image ("diskN", even if "diskNsM" was supplied), "ejects" the
// disk image from the system, and places the disk image file into the Trash.
// If at any step an error occurs, further processing is aborted.
void EjectAndTrashDiskImage(const std::string& dmg_bsd_device_name);

#endif  // CHROME_BROWSER_MAC_INSTALL_FROM_DMG_H_
