// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_ROUTINE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_ROUTINE_H_

#include "base/bind.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"

namespace chromeos {
namespace network_diagnostics {

// Defines the key components of a network diagnostics routine. This class is
// expected to be implemented by every network diagnostics routine.
class NetworkDiagnosticsRoutine {
 public:
  NetworkDiagnosticsRoutine();
  NetworkDiagnosticsRoutine(const NetworkDiagnosticsRoutine&) = delete;
  NetworkDiagnosticsRoutine& operator=(const NetworkDiagnosticsRoutine&) =
      delete;
  virtual ~NetworkDiagnosticsRoutine();

  // Determines whether this test is capable of being run.
  virtual bool CanRun();

  // Analyze the results gathered by the function and execute the callback.
  virtual void AnalyzeResultsAndExecuteCallback() = 0;

 protected:
  void set_verdict(mojom::RoutineVerdict routine_verdict) {
    verdict_ = routine_verdict;
  }
  mojom::RoutineVerdict verdict() const { return verdict_; }

 private:
  mojom::RoutineVerdict verdict_ = mojom::RoutineVerdict::kNotRun;
  friend class NetworkDiagnosticsRoutineTest;
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_ROUTINE_H_
