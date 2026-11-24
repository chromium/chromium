// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_DUMP_WITHOUT_CRASHING_TESTER_RUST_SHIM_H_
#define BASE_TEST_DUMP_WITHOUT_CRASHING_TESTER_RUST_SHIM_H_

#include <stdint.h>

#include <memory>

namespace base::test::rust {

// Installs a fake dump function for `base::debug::DumpWithoutCrashing()` that
// counts how many times it was invoked, and resets throttling. The fake is
// uninstalled when this object is destroyed.
class DumpWithoutCrashingTesterRs {
 public:
  static std::unique_ptr<DumpWithoutCrashingTesterRs> Create();

  DumpWithoutCrashingTesterRs();
  DumpWithoutCrashingTesterRs(const DumpWithoutCrashingTesterRs&) = delete;
  DumpWithoutCrashingTesterRs& operator=(const DumpWithoutCrashingTesterRs&) =
      delete;
  ~DumpWithoutCrashingTesterRs();

  // Returns the number of dumps since this object was created.
  int32_t DumpCount() const;
};

}  // namespace base::test::rust

#endif  // BASE_TEST_DUMP_WITHOUT_CRASHING_TESTER_RUST_SHIM_H_
