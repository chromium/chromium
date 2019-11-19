// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/cpu_identity.h"

#include <algorithm>  // for std::lower_bound()
#include <string.h>

#include "base/cpu.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"

namespace metrics {

namespace internal {

const CpuUarchTableEntry kCpuUarchTable[] = {
    // These were found on various sources on the Internet. Main ones are:
    // http://instlatx64.atw.hu/ for CPUID to model name and
    // http://www.cpu-world.com for model name to microarchitecture
    {"06_09", "Banias"},
    {"06_0D", "Dothan"},
    {"06_0F", "Merom"},
    {"06_16", "Merom"},
    {"06_17", "Nehalem"},
    {"06_1A", "Nehalem"},
    {"06_1C", "Bonnell"},     // Atom
    {"06_1D", "Nehalem"},
    {"06_1E", "Nehalem"},
    {"06_1F", "Nehalem"},
    {"06_25", "Westmere"},
    {"06_26", "Bonnell"},     // Atom
    {"06_2A", "SandyBridge"},
    {"06_2C", "Westmere"},
    {"06_2D", "SandyBridge"},
    {"06_2E", "Nehalem"},
    {"06_2F", "Westmere"},
    {"06_36", "Saltwell"},    // Atom
    {"06_37", "Silvermont"},
    {"06_3A", "IvyBridge"},
    {"06_3C", "Haswell"},
    {"06_3D", "Broadwell"},
    {"06_3E", "IvyBridge"},
    {"06_3F", "Haswell"},
    {"06_45", "Haswell"},
    {"06_46", "Haswell"},
    {"06_47", "Broadwell"},   // Broadwell-H
    {"06_4C", "Airmont"},     // Braswell
    {"06_4D", "Silvermont"},  // Avoton/Rangely
    {"06_4E", "Skylake"},
    {"06_55", "Skylake"},     // Skylake-X
    {"06_56", "Broadwell"},   // Broadwell-DE
    {"06_5C", "Goldmont"},
    {"06_5E", "Skylake"},
    {"06_5F", "Goldmont"},    // Denverton
    {"06_7A", "GoldmontPlus"},
    {"06_8E", "Kabylake"},
    {"06_9E", "Kabylake"},
    {"0F_03", "Prescott"},
    {"0F_04", "Prescott"},
    {"0F_06", "Presler"},
    {"15_70", "Excavator"},   // AMD Stoney Ridge
};

const CpuUarchTableEntry* kCpuUarchTableEnd =
    kCpuUarchTable + base::size(kCpuUarchTable);

bool CpuUarchTableCmp(const CpuUarchTableEntry& a,
                      const CpuUarchTableEntry& b) {
  return strcmp(a.family_model, b.family_model) < 0;
}

}  // namespace internal

CPUIdentity::CPUIdentity() : family(0), model(0) {}

CPUIdentity::CPUIdentity(const CPUIdentity& other) = default;

CPUIdentity::~CPUIdentity() {}

std::string GetCpuUarch(const CPUIdentity& cpuid) {
  if (cpuid.vendor != "GenuineIntel" && cpuid.vendor != "AuthenticAMD")
    return std::string();  // Non-Intel or -AMD

  std::string family_model =
      base::StringPrintf("%02X_%02X", cpuid.family, cpuid.model);
  const internal::CpuUarchTableEntry search_elem = {family_model.c_str(), ""};
  auto* bound = std::lower_bound(internal::kCpuUarchTable,
                                 internal::kCpuUarchTableEnd, search_elem,
                                 internal::CpuUarchTableCmp);
  if (bound == internal::kCpuUarchTableEnd ||
      bound->family_model != family_model)
    return std::string();  // Unknown uarch
  return bound->uarch;
}

CPUIdentity GetCPUIdentity() {
  CPUIdentity result = {};
  result.arch = base::SysInfo::OperatingSystemArchitecture();
  result.release = base::SysInfo::OperatingSystemVersion();
  base::CPU cpuid;
  result.vendor = cpuid.vendor_name();
  result.family = cpuid.family();
  result.model = cpuid.model();
  result.model_name = cpuid.cpu_brand();
  return result;
}

std::string SimplifyCPUModelName(const std::string& model_name) {
  std::string result = model_name;
  std::replace(result.begin(), result.end(), ' ', '-');
  base::ReplaceSubstringsAfterOffset(&result, 0, "(R)", "");
  return base::ToLowerASCII(result);
}

}  // namespace metrics
