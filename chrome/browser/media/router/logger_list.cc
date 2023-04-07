// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/logger_list.h"

#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_thread.h"

namespace media_router {

// static
LoggerList* LoggerList::GetInstance() {
  static LoggerList* instance = new LoggerList();
  return instance;
}

void LoggerList::AddLogger(LoggerImpl* logger_impl) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(logger_impl);
  loggers_.insert(logger_impl);
}

void LoggerList::RemoveLogger(LoggerImpl* logger_impl) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(logger_impl);
  loggers_.erase(logger_impl);
}

void LoggerList::Log(LoggerImpl::Severity severity,
                     mojom::LogCategory category,
                     const std::string& component,
                     const std::string& message,
                     const std::string& sink_id,
                     const std::string& media_source,
                     const std::string& session_id) {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    LogOnUiThread(severity, category, base::Time::Now(), component, message,
                  sink_id, media_source, session_id);
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&LoggerList::LogOnUiThread, base::Unretained(this),
                       severity, category, base::Time::Now(), component,
                       message, sink_id, media_source, session_id));
  }
}

int LoggerList::GetLoggerCount() const {
  return loggers_.size();
}

LoggerList::LoggerList() = default;

LoggerList::~LoggerList() = default;

void LoggerList::LogOnUiThread(LoggerImpl::Severity severity,
                               mojom::LogCategory category,
                               base::Time time,
                               const std::string& component,
                               const std::string& message,
                               const std::string& sink_id,
                               const std::string& media_source,
                               const std::string& session_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (LoggerImpl* logger : loggers_) {
    logger->Log(severity, category, time, component, message, sink_id,
                media_source, session_id);
  }
}

}  // namespace media_router
