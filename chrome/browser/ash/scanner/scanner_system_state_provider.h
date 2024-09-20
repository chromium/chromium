// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNER_SCANNER_SYSTEM_STATE_PROVIDER_H_
#define CHROME_BROWSER_ASH_SCANNER_SCANNER_SYSTEM_STATE_PROVIDER_H_

namespace ash {
struct ScannerSystemState;
}  // namespace ash

// Encapsulates all enable / disable logic required for Scanner. This includes
// a number of different checks that will require access to a valid profile
// instance. This includes the user's current consent status for Scanner, their
// preferences set, among other checks. Please see b/363103871 for more details.
class ScannerSystemStateProvider {
 public:
  ScannerSystemStateProvider();
  ~ScannerSystemStateProvider();

  ash::ScannerSystemState GetSystemState() const;
};

#endif  // CHROME_BROWSER_ASH_SCANNER_SCANNER_SYSTEM_STATE_PROVIDER_H_
