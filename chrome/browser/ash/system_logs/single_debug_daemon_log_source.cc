// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/single_debug_daemon_log_source.h"

#include <memory>

#include "base/functional/bind.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

using SupportedSource = SingleDebugDaemonLogSource::SupportedSource;

// Converts a logs source type to the corresponding debugd log name.
std::string GetLogName(SupportedSource source_type) {
  switch (source_type) {
    case SupportedSource::kModetest:
      return "modetest";
    case SupportedSource::kLsusb:
      return "lsusb";
    case SupportedSource::kLspci:
      return "lspci";
    case SupportedSource::kIfconfig:
      return "ifconfig";
    case SupportedSource::kUptime:
      return "uptime";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

}  // namespace

SingleDebugDaemonLogSource::SingleDebugDaemonLogSource(
    SupportedSource source_type)
    : SystemLogsSource(GetLogName(source_type)) {}

SingleDebugDaemonLogSource::~SingleDebugDaemonLogSource() {}

void SingleDebugDaemonLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  ash::DebugDaemonClient::Get()->GetLog(
      source_name(),
      base::BindOnce(&SingleDebugDaemonLogSource::OnFetchComplete,
                     weak_ptr_factory_.GetWeakPtr(), source_name(),
                     std::move(callback)));
}

void SingleDebugDaemonLogSource::OnFetchComplete(
    const std::string& log_name,
    SysLogsSourceCallback callback,
    std::optional<std::string> result) const {
  // |result| and |response| are the same type, but |result| is passed in from
  // DebugDaemonClient, which does not use the SystemLogsResponse alias.
  auto response = std::make_unique<SystemLogsResponse>();
  // Return an empty result if the call to GetLog() failed.
  if (result.has_value())
    response->emplace(log_name, result.value());

  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
