// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/logging.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"

namespace dictation {

namespace {
namespace dictation_api = extensions::api::dictation_private;
}  // namespace

DictationLogBuffer::DictationLogBuffer(content::BrowserContext* context)
    : context_(CHECK_DEREF(context)) {}

DictationLogBuffer::~DictationLogBuffer() = default;

void DictationLogBuffer::AddLog(std::string_view message) {
  buffer_.push_back(
      LogEntry{.timestamp = base::Time::Now().InMillisecondsFSinceUnixEpoch(),
               .message = std::string(message)});

  if (!flush_timer_.IsRunning()) {
    flush_timer_.Start(
        FROM_HERE, kFlushInterval,
        base::BindOnce(&DictationLogBuffer::Flush, base::Unretained(this)));
  }
}

void DictationLogBuffer::Flush() {
  flush_timer_.Stop();
  CHECK(!buffer_.empty());

  std::vector<LogEntry> entries;
  entries.swap(buffer_);

  auto* event_router = extensions::EventRouter::Get(&context_.get());
  if (!event_router) {
    return;
  }

  std::vector<dictation_api::LogMessageDetails> details_list;
  details_list.reserve(entries.size());
  for (const auto& entry : entries) {
    auto& details = details_list.emplace_back();
    details.timestamp = entry.timestamp;
    details.message = entry.message;
  }

  auto event_args = dictation_api::OnBrowserLog::Create(details_list);
  auto event = std::make_unique<extensions::Event>(
      extensions::events::DICTATION_PRIVATE_ON_BROWSER_LOG_EVENT,
      dictation_api::OnBrowserLog::kEventName, std::move(event_args),
      &context_.get());
  event_router->BroadcastEvent(std::move(event));
}

DictationLogEntry::DictationLogEntry(content::BrowserContext* context)
    : context_(CHECK_DEREF(context)) {}

DictationLogEntry::~DictationLogEntry() {
  std::string msg = stream_.str();
  VLOG(1) << "[VoiceTyping:Browser] " << msg;
  DictationKeyedService* service = DictationKeyedService::Get(&context_.get());
  if (service) {
    service->log_buffer().AddLog(msg);
  }
}

}  // namespace dictation
