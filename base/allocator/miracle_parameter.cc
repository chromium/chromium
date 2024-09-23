// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/miracle_parameter.h"

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"

namespace base::miracle_parameter {

std::string GetParamNameWithSuffix(const std::string& param_name) {
  // `base::SysInfo::AmountOfPhysicalMemoryMB()` refers to CommandLine
  // internally. If the CommandLine is not initialized, we return early to avoid
  // a crash.
  if (!base::CommandLine::InitializedForCurrentProcess()) {
    return param_name;
  }
  int physical_memory_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  const char* suffix =
      physical_memory_mb < kMiracleParameterMemory512MB  ? "ForLessThan512MB"
      : physical_memory_mb < kMiracleParameterMemory1GB  ? "For512MBTo1GB"
      : physical_memory_mb < kMiracleParameterMemory2GB  ? "For1GBTo2GB"
      : physical_memory_mb < kMiracleParameterMemory4GB  ? "For2GBTo4GB"
      : physical_memory_mb < kMiracleParameterMemory8GB  ? "For4GBTo8GB"
      : physical_memory_mb < kMiracleParameterMemory16GB ? "For8GBTo16GB"
                                                         : "For16GBAndAbove";
  return base::StrCat({param_name, suffix});
}

std::string GetMiracleParameterAsString(const base::Feature& feature,
                                        const std::string& param_name,
                                        const std::string& default_value) {
  return GetFieldTrialParamByFeatureAsString(
      feature, GetParamNameWithSuffix(param_name),
      GetFieldTrialParamByFeatureAsString(feature, param_name, default_value));
}

double GetMiracleParameterAsDouble(const base::Feature& feature,
                                   const std::string& param_name,
                                   double default_value) {
  return base::GetFieldTrialParamByFeatureAsDouble(
      feature, GetParamNameWithSuffix(param_name),
      base::GetFieldTrialParamByFeatureAsDouble(feature, param_name,
                                                default_value));
}

int GetMiracleParameterAsInt(const base::Feature& feature,
                             const std::string& param_name,
                             int default_value) {
  return base::GetFieldTrialParamByFeatureAsInt(
      feature, GetParamNameWithSuffix(param_name),
      base::GetFieldTrialParamByFeatureAsInt(feature, param_name,
                                             default_value));
}

bool GetMiracleParameterAsBool(const base::Feature& feature,
                               const std::string& param_name,
                               bool default_value) {
  return base::GetFieldTrialParamByFeatureAsBool(
      feature, GetParamNameWithSuffix(param_name),
      base::GetFieldTrialParamByFeatureAsBool(feature, param_name,
                                              default_value));
}

base::TimeDelta GetMiracleParameterAsTimeDelta(const base::Feature& feature,
                                               const std::string& param_name,
                                               base::TimeDelta default_value) {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      feature, GetParamNameWithSuffix(param_name),
      base::GetFieldTrialParamByFeatureAsTimeDelta(feature, param_name,
                                                   default_value));
}

}  // namespace base::miracle_parameter
