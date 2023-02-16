// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/crosapi_system_log_source.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"

namespace system_logs {

namespace {
constexpr char kLacrosLogEntryPrefix[] = "Lacros ";
}  // namespace

CrosapiSystemLogSource::CrosapiSystemLogSource()
    : SystemLogsSource("LacrosSystemLog") {
  crosapi::BrowserManager::Get()->AddObserver(this);
}

CrosapiSystemLogSource::~CrosapiSystemLogSource() {
  crosapi::BrowserManager::Get()->RemoveObserver(this);
}

void CrosapiSystemLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  if (crosapi::BrowserManager::Get()->IsRunning()) {
    callback_ = std::move(callback);
    crosapi::BrowserManager::Get()->GetFeedbackData(
        base::BindOnce(&CrosapiSystemLogSource::OnGetFeedbackData,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Fetch is called right after the data source is added to the
    // SystemLogsFetcher when Lacros is running, it is unlikely Lacros will
    // be terminated before Fetch is called. But it does not hurt to check
    // again and handle the case for playing safely.
    std::move(callback).Run(std::make_unique<SystemLogsResponse>());
  }
}

void CrosapiSystemLogSource::OnGetFeedbackData(base::Value::Dict system_infos) {
  auto response = std::make_unique<SystemLogsResponse>();
  for (const auto item : system_infos) {
    std::string log_entry_key = kLacrosLogEntryPrefix + item.first;
    if (item.second.is_string()) {
      response->emplace(log_entry_key, item.second.GetString());
    } else {
      LOG(ERROR) << "Failed to retrieve the content for log entry: "
                 << log_entry_key;
    }
  }
  std::move(callback_).Run(std::move(response));
}

void CrosapiSystemLogSource::OnMojoDisconnected() {
  if (callback_.is_null())
    return;

  // Run callback_ with empty response.
  std::move(callback_).Run(std::make_unique<SystemLogsResponse>());
}

}  //  namespace system_logs
