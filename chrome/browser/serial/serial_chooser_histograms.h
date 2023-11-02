// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_HISTOGRAMS_H_
#define CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_HISTOGRAMS_H_

// Reasons why the chooser was closed. This enum is used in histograms so do not
// remove or reorder entries. Only add at the end and update kMaxValue. Also
// remember to update the enum listing in tools/metrics/histograms/enums.xml.
enum class SerialChooserOutcome {
  kCancelled = 0,
  kCancelledNoDevices,
  kPermissionGranted,
  kEphemeralPermissionGranted,
  kMaxValue = kEphemeralPermissionGranted,
};

// Types of permission revocation that can happen. This enum is used in
// histograms so do not remove or reorder entries. Only add at the end and
// update kMaxValue. Also remember to update the enum listing in
// tools/metrics/histograms/enums.xml.
enum class SerialPermissionRevoked {
  kPersistentByUser = 0,
  kEphemeralByUser,
  kEphemeralByDisconnect,
  kPersistentByWebsite,
  kEphemeralByWebsite,
  kMaxValue = kEphemeralByWebsite,
};

#endif  // CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_HISTOGRAMS_H_
