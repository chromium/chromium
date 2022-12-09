// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/cpu_identity.h"

#include <algorithm>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(CpuIdentityTest, CpuUarchTableIsSorted) {
  EXPECT_TRUE(std::is_sorted(
      internal::kCpuUarchTable,
      internal::kCpuUarchTableEnd,
      internal::CpuUarchTableCmp));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnUarch_IvyBridge) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x3a;  // IvyBridge
  cpuid.model_name = "";
  EXPECT_EQ("IvyBridge", GetCpuUarch(cpuid));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnUarch_SandyBridge) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x2a;  // SandyBridge
  cpuid.model_name = "";
  EXPECT_EQ("SandyBridge", GetCpuUarch(cpuid));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnUarch_GoldmontPlus) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x7a;  // Gemini Lake
  cpuid.model_name = "";
  EXPECT_EQ("GoldmontPlus", GetCpuUarch(cpuid));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnUarch_Tigerlake) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x8c;  // Tiger Lake
  cpuid.model_name = "";
  EXPECT_EQ("Tigerlake", GetCpuUarch(cpuid));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnUarch_Excavator) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "AuthenticAMD";
  cpuid.family = 0x15;
  cpuid.model = 0x70;  // Excavator
  cpuid.model_name = "";
  EXPECT_EQ("Excavator", GetCpuUarch(cpuid));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnUarch_Zen) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "AuthenticAMD";
  cpuid.family = 0x17;
  cpuid.model = 0x01;
  cpuid.model_name = "";
  EXPECT_EQ("Zen", GetCpuUarch(cpuid));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnUarch_Zen2) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "AuthenticAMD";
  cpuid.family = 0x17;
  cpuid.model = 0x60;
  cpuid.model_name = "";
  EXPECT_EQ("Zen2", GetCpuUarch(cpuid));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnArch_x86_32) {
  CPUIdentity cpuid;
  cpuid.arch = "x86";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x2f;  // Westmere
  cpuid.model_name = "";
  EXPECT_EQ("Westmere", GetCpuUarch(cpuid));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnArch_Unknown) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "NotIntelOrAmd";
  cpuid.family = 0;
  cpuid.model = 0;
  cpuid.model_name = "";
  EXPECT_EQ("", GetCpuUarch(cpuid));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnArch_UnknownUpperBound) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0xff;
  cpuid.model = 0xff;
  cpuid.model_name = "";
  EXPECT_EQ("", GetCpuUarch(cpuid));
}

TEST(CpuIdentityTest, SimplifyCPUModelName) {
  EXPECT_EQ("", SimplifyCPUModelName(""));
  EXPECT_EQ("intel-celeron-2955u-@-1.40ghz",
            SimplifyCPUModelName("Intel(R) Celeron(R) 2955U @ 1.40GHz"));
  EXPECT_EQ("armv7-processor-rev-3-(v7l)",
            SimplifyCPUModelName("ARMv7 Processor rev 3 (v7l)"));
}

}  // namespace metrics
