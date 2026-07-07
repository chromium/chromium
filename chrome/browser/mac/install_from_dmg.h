// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_INSTALL_FROM_DMG_H_
#define CHROME_BROWSER_MAC_INSTALL_FROM_DMG_H_

#include <string>

enum class DiskImageStatus {
  kFailure,
  kNotRunningFromDiskImage,
  kRunningFromDiskImage
};

struct RunningFromDiskImageResult {
  DiskImageStatus status;

  // The BSD device name for the disk image's device, in "diskNsM" form. It is
  // set if the status is `kRunningFromDiskImage`.
  std::string dmg_bsd_device_name;
};

// Returns info about the application is located on a read-only filesystem of a
// disk image.
RunningFromDiskImageResult IsAppRunningFromReadOnlyDiskImage();

// If the application is running from a read-only disk image, prompts the user
// to install it to the hard drive.
//
// If the user approves, the application
// will be installed and launched, and MaybeInstallFromDiskImage will return
// true. In that case, the caller must exit expeditiously.
//
// Returns false if the user does not approve, or if any error occurs.
bool MaybeInstallFromDiskImage();

// Given a BSD device name of the form "diskN" or "diskNsM" as used by IOKit,
// where the device name corresponds to a disk image, unmounts all filesystems
// on that disk image ("diskN", even if "diskNsM" was supplied), "ejects" the
// disk image from the system, and places the disk image file into the Trash.
// If at any step an error occurs, further processing is aborted.
void EjectAndTrashDiskImage(const std::string& dmg_bsd_device_name);

#endif  // CHROME_BROWSER_MAC_INSTALL_FROM_DMG_H_
