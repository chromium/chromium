// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_METRICS_H_
#define CHROME_BROWSER_MAC_METRICS_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"

namespace mac_metrics {

// Don't renumber these values. They are recorded in UMA metrics.
// See enum MacFileSystemType in enums.xml.
enum class FileSystemType {
  kHFS = 0,
  kAPFS = 1,
  kUnknown = 2,
  kMaxValue = kUnknown,
};

// A class for collecting mac related UMA metrics.
class Metrics : UpgradeObserver {
 public:
  Metrics();
  Metrics(const Metrics&) = delete;
  Metrics& operator=(const Metrics&) = delete;
  ~Metrics() override;

  // Records the file system type where the running instance of Chrome is
  // installed.
  void RecordAppFileSystemType();

  // Records the validation status of the running instance of Chrome. Intended
  // to be run after an upgrade. Typically invoked by OnUpgradeRecommended which
  // is called 1 hour after the update has been staged. The validation and
  // metric recording take place asynchronously. The closure will be called
  // after the metric has been recorded or if an error occurred.
  void RecordAppUpgradeCodeSignatureValidation(base::OnceClosure closure);

 private:
  // UpgradeObserver
  void OnUpgradeRecommended() override;
};

}  // namespace mac_metrics

#endif  // CHROME_BROWSER_MAC_METRICS_H_
