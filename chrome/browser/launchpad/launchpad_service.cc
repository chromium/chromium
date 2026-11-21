// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/launchpad/launchpad_service.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/profiles/profile.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "url/gurl.h"

namespace launchpad {

namespace {

std::optional<GURL> ParseHttpOrHttpsUrl(std::string_view spec) {
  GURL url(spec);
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }
  return url;
}

std::optional<GURL> ExtractGmailUrl(
    const personal_context::proto::GmailReference& gmail) {
  if (std::optional<GURL> url =
          ParseHttpOrHttpsUrl(gmail.message_urls().desktop_web_url())) {
    return url;
  }
  return ParseHttpOrHttpsUrl(gmail.message_url());
}

base::Time ProtoTimestampToTime(
    const personal_context::proto::Timestamp& timestamp) {
  // Bounds from `google.protobuf.Timestamp`:
  // 0001-01-01T00:00:00Z to 9999-12-31T23:59:59.999999999Z.
  static constexpr int64_t kMinTimestampSeconds = -62135596800;
  static constexpr int64_t kMaxTimestampSeconds = 253402300799;
  static constexpr int32_t kMaxTimestampNanos = 999'999'999;

  if (timestamp.seconds() < kMinTimestampSeconds ||
      timestamp.seconds() > kMaxTimestampSeconds || timestamp.nanos() < 0 ||
      timestamp.nanos() > kMaxTimestampNanos) {
    return base::Time();
  }
  if (timestamp.seconds() == 0 && timestamp.nanos() == 0) {
    return base::Time();
  }
  return base::Time::UnixEpoch() + base::Seconds(timestamp.seconds()) +
         base::Nanoseconds(timestamp.nanos());
}

LaunchpadTask::Status ToLaunchpadTaskStatus(
    personal_context::proto::AutoTodoItem::Status status) {
  switch (status) {
    case personal_context::proto::AutoTodoItem::STATUS_COMPLETED:
      return LaunchpadTask::Status::kCompleted;
    case personal_context::proto::AutoTodoItem::STATUS_DISMISSED:
      return LaunchpadTask::Status::kDismissed;
    case personal_context::proto::AutoTodoItem::STATUS_UNSPECIFIED:
    case personal_context::proto::AutoTodoItem::STATUS_ACTIVE:
    default:
      return LaunchpadTask::Status::kActive;
  }
}

LaunchpadTask ToLaunchpadTask(
    const personal_context::proto::AutoTodoItem& todo) {
  if (todo.id().empty()) {
    return {};
  }

  LaunchpadTask task;
  task.id = todo.id();
  task.title = todo.title();
  task.description = todo.description();
  task.importance_score = todo.importance_score();
  task.status = ToLaunchpadTaskStatus(todo.status());
  task.actionable_url = ParseHttpOrHttpsUrl(todo.actionable_url());

  for (const personal_context::proto::SourceReference& ref :
       todo.source_references()) {
    if (ref.has_gmail()) {
      if (std::optional<GURL> url = ExtractGmailUrl(ref.gmail())) {
        task.source_references.push_back(GmailReference{
            .url = *std::move(url),
            .subject = std::string(ref.gmail().subject()),
        });
      }
    } else if (ref.has_chrome_history_entry()) {
      const personal_context::proto::ChromeHistoryEntry& history_proto =
          ref.chrome_history_entry();
      if (std::optional<GURL> url = ParseHttpOrHttpsUrl(history_proto.url())) {
        ChromeHistoryEntry history_entry{
            .url = *std::move(url),
            .title = std::string(history_proto.title()),
        };
        if (history_proto.has_visit_timestamp()) {
          history_entry.visit_timestamp =
              ProtoTimestampToTime(history_proto.visit_timestamp());
        }
        task.source_references.push_back(std::move(history_entry));
      }
    }
  }
  return task;
}

}  // namespace

LaunchpadService::LaunchpadService(
    Profile* profile,
    personal_context::PersonalContextService* personal_context_service)
    : profile_(CHECK_DEREF(profile)),
      personal_context_service_(CHECK_DEREF(personal_context_service)) {}

LaunchpadService::~LaunchpadService() = default;

void LaunchpadService::GenerateTasks(GenerateTasksCallback callback) {
  if (is_generating_tasks_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  is_generating_tasks_ = true;

  personal_context::proto::AutoTodosRequest request_metadata;
  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
      request_metadata, /*options=*/{},
      base::BindOnce(&LaunchpadService::OnTasksFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

bool LaunchpadService::IsGeneratingTasks() const {
  return is_generating_tasks_;
}

void LaunchpadService::GetAllTasks(
    TasksStore::GetAllTasksCallback callback) const {
  // TODO(b/586300169): Read from server cache when implemented.
  tasks_store_.GetAllTasks(std::move(callback));
}

bool LaunchpadService::RunTask(const std::string& prompt,
                               BrowserWindowInterface* browser_window) {
  return RunTaskForProfile(&profile_.get(), prompt, browser_window);
}

// static
bool LaunchpadService::RunTaskForProfile(
    Profile* profile,
    const std::string& prompt,
    BrowserWindowInterface* browser_window) {
  if (prompt.empty() || !glic::GlicEnabling::IsEnabledForProfile(profile)) {
    return false;
  }

  glic::GlicKeyedService* glic_service = glic::GlicKeyedService::Get(profile);
  if (!glic_service) {
    return false;
  }

  glic::GlicInvokeOptions options(
      glic::mojom::InvocationSource::kContextHubAutoTodos);
  options.prompts.push_back(prompt);
  options.target.conversation = glic::NewConversation();
  options.feature_mode = glic::mojom::FeatureMode::kActuation;
  options.target.actuation_target = glic::mojom::ActuationTarget::kAgentDecides;
  options.target.surface = glic::DefaultSurface{browser_window};

  glic::GlicInvokeWithAutoSubmitOptions auto_submit_options;
  auto_submit_options.show_panel = true;

  return static_cast<bool>(glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      std::move(options), std::move(auto_submit_options)));
}

void LaunchpadService::OnTasksFetched(
    GenerateTasksCallback callback,
    personal_context::FetchContextResult result) {
  is_generating_tasks_ = false;

  if (!result.response.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  personal_context::proto::AutoTodosResponse response;
  if (!response.ParseFromString(result.response->value())) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Clear previously cached tasks as we perform a fresh regeneration on each
  // trigger.
  tasks_store_.Clear(base::DoNothing());

  std::vector<LaunchpadTask> tasks =
      base::ToVector(response.todos(), &ToLaunchpadTask);

  tasks_store_.AddAllTasks(tasks, base::DoNothing());
  tasks_store_.GetAllTasks(base::BindOnce(
      [](GenerateTasksCallback callback,
         std::vector<LaunchpadTask> stored_tasks) {
        std::move(callback).Run(std::move(stored_tasks));
      },
      std::move(callback)));
}

}  // namespace launchpad
