// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/iwlwifi_dump_log_source.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace system_logs {

namespace {

constexpr char kIwlwifiDumpLocation[] = "/var/log/last_iwlwifi_dump";

std::unique_ptr<SystemLogsResponse> CheckExistenceOnBlockingTaskRunner() {
  auto result = std::make_unique<SystemLogsResponse>();
  if (base::PathExists(base::FilePath(kIwlwifiDumpLocation))) {
    result->emplace(
        kIwlwifiDumpKey,
        l10n_util::GetStringUTF8(IDS_FEEDBACK_IWLWIFI_DEBUG_DUMP_EXPLAINER));
  }
  return result;
}

std::unique_ptr<SystemLogsResponse> ReadDumpOnBlockingTaskRunner() {
  auto result = std::make_unique<SystemLogsResponse>();
  std::string contents;
  if (base::ReadFileToString(base::FilePath(kIwlwifiDumpLocation), &contents))
    result->emplace(kIwlwifiDumpKey, std::move(contents));
  return result;
}

}  // namespace

IwlwifiDumpChecker::IwlwifiDumpChecker()
    : SystemLogsSource("IwlwifiDumpChecker") {}

IwlwifiDumpChecker::~IwlwifiDumpChecker() = default;

void IwlwifiDumpChecker::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::TaskTraits({base::ThreadPool(), base::MayBlock(),
                        base::TaskPriority::BEST_EFFORT}),
      base::BindOnce(&CheckExistenceOnBlockingTaskRunner),
      base::BindOnce(std::move(callback)));
}

IwlwifiDumpLogSource::IwlwifiDumpLogSource()
    : SystemLogsSource("IwlwifiDump") {}

IwlwifiDumpLogSource::~IwlwifiDumpLogSource() = default;

void IwlwifiDumpLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::TaskTraits({base::ThreadPool(), base::MayBlock(),
                        base::TaskPriority::BEST_EFFORT}),
      base::BindOnce(&ReadDumpOnBlockingTaskRunner),
      base::BindOnce(std::move(callback)));
}

bool ContainsIwlwifiLogs(const FeedbackCommon::SystemLogsMap* sys_logs) {
  return sys_logs->count(kIwlwifiDumpKey);
}

}  // namespace system_logs
