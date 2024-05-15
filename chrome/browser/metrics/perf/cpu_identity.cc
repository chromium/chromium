// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/cpu_identity.h"

#include <string.h>

#include <algorithm>  // for std::lower_bound()

#include "base/cpu.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace metrics {

namespace internal {

const CpuUarchTableEntry kCpuUarchTable[] = {
    // These were found on various sources on the Internet. Main ones are:
    // http://instlatx64.atw.hu/ for CPUID to model name and
    // http://www.cpu-world.com for model name to microarchitecture
    // clang-format off
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
    {"06_7E", "IceLake"},
    {"06_8C", "Tigerlake"},
    {"06_8D", "Tigerlake"},
    {"06_8E", "Kabylake"},
    {"06_97", "AlderLake"},
    {"06_9A", "AlderLake"},
    {"06_9C", "Tremont"},     // Jasperlake
    {"06_9E", "Kabylake"},
    {"06_A5", "CometLake"},
    {"06_A6", "CometLake"},
    {"06_B7", "RaptorLake"},
    {"06_BA", "RaptorLake"},
    {"06_BE", "Gracemont"},   // Alderlake-N
    {"06_BF", "RaptorLake"},
    {"0F_03", "Prescott"},
    {"0F_04", "Prescott"},
    {"0F_06", "Presler"},
    {"0F_68", "Athlon64"},
    {"0F_6B", "Athlon64"},
    {"10_02", "K10"},
    {"10_04", "K10"},
    {"10_05", "K10"},
    {"10_06", "K10"},
    {"12_01", "Llano"},
    {"14_01", "Bobcat"},
    {"14_02", "Bobcat"},
    {"15_01", "Bulldozer"},
    {"15_10", "Piledriver"},  // AMD Trinity
    {"15_13", "Piledriver"},  // AMD Richland
    {"15_30", "Steamroller"}, // AMD Kaveri
    {"15_38", "Steamroller"}, // AMD Godavari
    {"15_60", "Excavator"},   // AMD Carizzo
    {"15_65", "Excavator"},   // AMD Bristol Ridge
    {"15_70", "Excavator"},   // AMD Stoney Ridge
    {"16_00", "Jaguar"},      // AMD APUs Beema, Mullins, Steppe & Crowned Eagle
    {"16_30", "Puma"},        // AMD APUs Kabini, Temash, Kyoto
    {"17_01", "Zen"},         // AMD Summit Ridge
    {"17_08", "Zen+"},        // AMD Pinacle Ridge
    {"17_11", "Zen"},         // AMD Raven Ridge
    {"17_18", "Zen+"},        // AMD Picasso
    {"17_20", "Zen"},         // AMD Dali
    {"17_60", "Zen2"},        // AMD Renoir
    {"17_68", "Zen2"},        // AMD Lucienne
    {"17_71", "Zen2"},        // AMD Matisse
    {"17_A0", "Zen2"},        // AMD Mendocino
    {"19_50", "Zen3"},        // AMD Cezanne
    // clang-format on
};

const CpuUarchTableEntry* kCpuUarchTableEnd =
    kCpuUarchTable + std::size(kCpuUarchTable);

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
  result.release =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      base::SysInfo::KernelVersion();
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
      base::SysInfo::OperatingSystemVersion();
#else
#error "Unsupported configuration"
#endif
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
