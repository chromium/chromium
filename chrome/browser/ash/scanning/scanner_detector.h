// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_SCANNER_DETECTOR_H_
#define CHROME_BROWSER_ASH_SCANNING_SCANNER_DETECTOR_H_

#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/components/scanning/scanner.h"
#include "chromeos/chromeos_export.h"

namespace ash {

// Interface for automatic scanner detection. This API allows for incremental
// discovery of scanners and provides a notification when discovery is complete.
//
// All of the interface calls in this class must be called from the same
// sequence but do not have to be on any specific thread.
//
// The usual usage of this interface by a class that wants to maintain an
// up-to-date list of detected scanners is:
//
// auto detector_ = ScannerDetectorImplementation::Create();
// detector_->RegisterScannersDetectedCallback(callback);
// scanners_ = detector_->GetScanners();
//
class CHROMEOS_EXPORT ScannerDetector {
 public:
  virtual ~ScannerDetector() = default;

  // Registers the callback used to provide notifications when scanners are
  // detected.
  using OnScannersDetectedCallback =
      base::RepeatingCallback<void(std::vector<Scanner> scanners)>;
  virtual void RegisterScannersDetectedCallback(
      OnScannersDetectedCallback callback) = 0;

  // Returns the detected scanners.
  virtual std::vector<Scanner> GetScanners() = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_SCANNER_DETECTOR_H_
