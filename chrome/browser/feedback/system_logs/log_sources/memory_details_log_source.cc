// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"

#include "chrome/browser/memory_details.h"
#include "components/feedback/feedback_report.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

// Reads Chrome memory usage.
class SystemLogsMemoryHandler : public MemoryDetails {
 public:
  explicit SystemLogsMemoryHandler(SysLogsSourceCallback callback)
      : callback_(std::move(callback)) {}

  SystemLogsMemoryHandler(const SystemLogsMemoryHandler&) = delete;
  SystemLogsMemoryHandler& operator=(const SystemLogsMemoryHandler&) = delete;

  // Sends the data to the callback.
  // MemoryDetails override.
  void OnDetailsAvailable() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    auto response = std::make_unique<SystemLogsResponse>();
    (*response)["mem_usage"] = ToLogString(/*include_tab_title=*/false);
    (*response)[feedback::FeedbackReport::kMemUsageWithTabTitlesKey] =
        ToLogString(/*include_tab_title=*/true);
    DCHECK(!callback_.is_null());
    std::move(callback_).Run(std::move(response));
  }

 private:
  ~SystemLogsMemoryHandler() override {}
  SysLogsSourceCallback callback_;
};

MemoryDetailsLogSource::MemoryDetailsLogSource()
    : SystemLogsSource("MemoryDetails") {
}

MemoryDetailsLogSource::~MemoryDetailsLogSource() {
}

void MemoryDetailsLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  scoped_refptr<SystemLogsMemoryHandler> handler(
      new SystemLogsMemoryHandler(std::move(callback)));
  handler->StartFetch();
}

}  // namespace system_logs
