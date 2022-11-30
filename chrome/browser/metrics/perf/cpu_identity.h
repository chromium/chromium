// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_CPU_IDENTITY_H_
#define CHROME_BROWSER_METRICS_PERF_CPU_IDENTITY_H_

#include <string>

namespace metrics {

// Struct containing the CPU identity fields used to choose perf commands.
// These are populated from base::CPU, but having them in a settable struct
// makes things testable.
struct CPUIdentity {
  CPUIdentity();
  CPUIdentity(const CPUIdentity& other);
  ~CPUIdentity();

  // The system architecture based on uname().
  // (Technically, not a property of the CPU.)
  std::string arch;
  // The kernel release version.
  std::string release;
  // CUID fields:
  std::string vendor;  // e.g. "GenuineIntel"
  int family;
  int model;
  // CPU model name. e.g. "Intel(R) Celeron(R) 2955U @ 1.40GHz"
  std::string model_name;
};

// Get the CPUIdentity based on the actual system.
CPUIdentity GetCPUIdentity();

// Return the CPU microarchitecture based on the family and model derived
// from |cpuid|, and kCpuUarchTable, or the empty string for unknown
// microarchitectures.
std::string GetCpuUarch(const CPUIdentity& cpuid);

// Simplify a CPU model name. The rules are:
// - Replace spaces with hyphens.
// - Strip all "(R)" symbols.
// - Convert to lower case.
std::string SimplifyCPUModelName(const std::string& model_name);

namespace internal {

// Exposed for unit testing.

struct CpuUarchTableEntry {
  const char *family_model;
  const char *uarch;
};

bool CpuUarchTableCmp(const CpuUarchTableEntry& a, const CpuUarchTableEntry& b);

extern const CpuUarchTableEntry kCpuUarchTable[];
extern const CpuUarchTableEntry* kCpuUarchTableEnd;

}  // namespace internal

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_CPU_IDENTITY_H_
