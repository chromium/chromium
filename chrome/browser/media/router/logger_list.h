// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_LOGGER_LIST_H_
#define CHROME_BROWSER_MEDIA_ROUTER_LOGGER_LIST_H_

#include <set>

#include "components/media_router/browser/logger_impl.h"
#include "content/public/browser/browser_thread.h"

namespace media_router {

// This class uses a list of LoggerImpl instances to handle logging from within
// MediaSinkService component. It allows a client to log to multiple LoggerImpl
// instances at once, where each instance is typically associated with a
// Profile. It is used as a singleton that is never freed. All methods must be
// called on the UI thread except for GetInstance() and Log() as it also handles
// logging for MediaSinkService classes that are on IO thread.
class LoggerList {
 public:
  // Can be called on any thread.
  // Returns the lazily-created singleton instance.
  static LoggerList* GetInstance();

  LoggerList(const LoggerList&) = delete;
  LoggerList& operator=(const LoggerList&) = delete;

  // Only on the UI thread.
  void AddLogger(LoggerImpl* logger_impl);
  void RemoveLogger(LoggerImpl* logger_impl);

  // Can be called on any thread.
  void Log(LoggerImpl::Severity severity,
           mojom::LogCategory category,
           const std::string& component,
           const std::string& message,
           const std::string& sink_id,
           const std::string& media_source,
           const std::string& session_id);

  // Used by tests.
  int GetLoggerCount() const;

 private:
  LoggerList();
  ~LoggerList();

  // Only on the UI thread.
  void LogOnUiThread(LoggerImpl::Severity severity,
                     mojom::LogCategory category,
                     base::Time time,
                     const std::string& component,
                     const std::string& message,
                     const std::string& sink_id,
                     const std::string& media_source,
                     const std::string& session_id);

  std::set<raw_ptr<LoggerImpl>> loggers_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_LOGGER_LIST_H_
