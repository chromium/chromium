// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_CROSAPI_SYSTEM_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_CROSAPI_SYSTEM_LOG_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Gathers Lacros system information log data via crosapi calls.
class CrosapiSystemLogSource : public SystemLogsSource,
                               public crosapi::BrowserManagerObserver {
 public:
  CrosapiSystemLogSource();
  ~CrosapiSystemLogSource() override;
  CrosapiSystemLogSource(const CrosapiSystemLogSource&) = delete;
  CrosapiSystemLogSource& operator=(const CrosapiSystemLogSource&) = delete;

  // SystemLogsSource
  void Fetch(SysLogsSourceCallback request) override;

 private:
  // Callback for getting lacros feedback data.
  void OnGetFeedbackData(base::Value::Dict system_infos);

  // crosapi::BrowserManagerObserver
  void OnMojoDisconnected() override;

  SysLogsSourceCallback callback_;
  base::WeakPtrFactory<CrosapiSystemLogSource> weak_ptr_factory_{this};
};

}  // namespace system_logs

#endif  //  CHROME_BROWSER_ASH_SYSTEM_LOGS_CROSAPI_SYSTEM_LOG_SOURCE_H_
