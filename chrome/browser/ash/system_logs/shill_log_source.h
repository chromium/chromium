// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_SHILL_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_SHILL_LOG_SOURCE_H_

#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Gathers Device and Service properties from Shill for system/feedback logs.
class ShillLogSource : public SystemLogsSource {
 public:
  explicit ShillLogSource(bool scrub);
  ~ShillLogSource() override;
  ShillLogSource(const ShillLogSource&) = delete;
  ShillLogSource& operator=(const ShillLogSource&) = delete;

  // SystemLogsSource
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  void OnGetManagerProperties(std::optional<base::Value::Dict> result);
  void OnGetDevice(const std::string& device_path,
                   std::optional<base::Value::Dict> properties);
  void AddDeviceAndRequestIPConfigs(const std::string& device_path,
                                    const base::Value::Dict& properties);
  void OnGetIPConfig(const std::string& device_path,
                     const std::string& ip_config_path,
                     std::optional<base::Value::Dict> properties);
  void AddIPConfig(const std::string& device_path,
                   const std::string& ip_config_path,
                   const base::Value::Dict& properties);
  void OnGetService(const std::string& service_path,
                    std::optional<base::Value::Dict> properties);
  // Scrubs |properties| for PII data based on the |object_path|. Also expands
  // UIData from JSON into a dictionary if present.
  base::Value::Dict ScrubAndExpandProperties(
      const std::string& object_path,
      const base::Value::Dict& properties);
  // Check whether all property requests have completed. If so, invoke
  // |callback_| and clear results.
  void CheckIfDone();

  const bool scrub_;
  SysLogsSourceCallback callback_;
  std::set<std::string> device_paths_;
  std::set<std::string> service_paths_;
  // More than one device may request the same IP configs, so use multiset.
  std::multiset<std::string> ip_config_paths_;
  base::Value::Dict devices_;
  base::Value::Dict services_;
  base::WeakPtrFactory<ShillLogSource> weak_ptr_factory_{this};
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_SHILL_LOG_SOURCE_H_
