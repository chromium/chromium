// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_SMART_CARD_HISTOGRAMS_H_
#define CHROME_BROWSER_SMART_CARD_SMART_CARD_HISTOGRAMS_H_

// There is another file with the same name, in
// `//content/browser/smart_card/smart_card_histograms.h`. The difference is
// that:
// - one-time permissions that are the main point of metrics collected here
//   are a feature that lives in `//chrome`, along with the specifics of the
//   smart card permission model
// - the other file handles the general behavior of the API and needs to be
//   accessed from `//content/browser/smart_card/smart_card_service.cc`.

// Reasons why the ephemeral permission might have expired.
enum SmartCardOneTimePermissionExpiryReason {
  // There's no window of the application left.
  kSmartCardPermissionExpiredLastWindowClosed = 0,
  // All windows of the application have been in the background for too long.
  kSmartCardPermissionExpiredAllWindowsInTheBackgroundTimeout,
  // Max lifetime of the one-time permission has been reached.
  kSmartCardPermissionExpiredMaxLifetimeReached,
  // System was suspended.
  kSmartCardPermissionExpiredSystemSuspended,
  // Smart card reader the grant referred to was removed.
  kSmartCardPermissionExpiredReaderRemoved,
  // Smart card was removed from the reader the grant referred to.
  kSmartCardPermissionExpiredCardRemoved,
  kSmartCardPermissionExpiredMax = kSmartCardPermissionExpiredReaderRemoved
};

void RecordSmartCardOneTimePermissionExpiryReason(
    SmartCardOneTimePermissionExpiryReason reason);

#endif  // CHROME_BROWSER_SMART_CARD_SMART_CARD_HISTOGRAMS_H_
