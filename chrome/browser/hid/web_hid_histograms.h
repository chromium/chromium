// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_WEB_HID_HISTOGRAMS_H_
#define CHROME_BROWSER_HID_WEB_HID_HISTOGRAMS_H_

// Reasons the chooser may be closed. These are used in histograms so do not
// remove/reorder entries. Only add at the end and update kMaxValue. Also
// remember to update the enum listing in tools/metrics/histograms/enums.xml.
enum class WebHidChooserClosed {
  // The user cancelled the permission prompt without selecting a device.
  kCancelled = 0,
  // The user probably cancelled the permission prompt without selecting a
  // device because there were no devices to select.
  kCancelledNoDevices,
  // The user granted permission to access a device.
  kPermissionGranted,
  // The user granted permission to access a device but that permission will be
  // revoked when the device is disconnected.
  kEphemeralPermissionGranted,
  // The chooser lost focus and closed itself.
  kLostFocus,
  kMaxValue = kLostFocus,
};

void RecordWebHidChooserClosure(WebHidChooserClosed disposition);

#endif  // CHROME_BROWSER_HID_WEB_HID_HISTOGRAMS_H_
