// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/child_process_task.h"

#include <utility>

#include "base/byte_count.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/process_resource_usage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"  // nogncheck
#include "extensions/browser/extension_registry.h"  // nogncheck
#include "extensions/common/extension_set.h"        // nogncheck
#endif

namespace task_manager {

namespace {

std::u16string GetLocalizedTitle(const std::u16string& title,
                                 int process_type,
                                 ChildProcessTask::ProcessSubtype subtype) {
  std::u16string result_title = title;

  // Explicitly mark name as LTR if there is no strong RTL character,
  // to avoid the wrong concatenation result similar to "!Yahoo Mail: the
  // best web-based Email: NIGULP", in which "NIGULP" stands for the Hebrew
  // or Arabic word for "plugin".
  base::i18n::AdjustStringForLocaleDirection(&result_title);

  switch (process_type) {
    case content::PROCESS_TYPE_UTILITY:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_UTILITY_PREFIX,
                                        result_title);
    case content::PROCESS_TYPE_GPU:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_GPU_PREFIX);
    case content::PROCESS_TYPE_PPAPI_PLUGIN_DEPRECATED:
    case content::PROCESS_TYPE_PPAPI_BROKER_DEPRECATED:
      NOTREACHED();
    case content::PROCESS_TYPE_RENDERER: {
      switch (subtype) {
        case ChildProcessTask::ProcessSubtype::kSpareRenderProcess:
          return l10n_util::GetStringUTF16(
              IDS_TASK_MANAGER_SPARE_RENDERER_PREFIX);
#if BUILDFLAG(ENABLE_GLIC)
        case ChildProcessTask::ProcessSubtype::kGlicRenderProcess:
          return l10n_util::GetStringUTF16(
              IDS_TASK_MANAGER_GLIC_RENDERER_PREFIX);
#endif
        case ChildProcessTask::ProcessSubtype::kUnknownRenderProcess:
          return l10n_util::GetStringUTF16(
              IDS_TASK_MANAGER_UNKNOWN_RENDERER_PREFIX);
        default:
          break;
      }
      [[fallthrough]];
    }
    // These types don't need display names or get them from elsewhere.
    case content::PROCESS_TYPE_BROWSER:
    case content::PROCESS_TYPE_ZYGOTE:
    case content::PROCESS_TYPE_SANDBOX_HELPER:
    case content::PROCESS_TYPE_MAX:
      break;
    case content::PROCESS_TYPE_UNKNOWN:
      NOTREACHED() << "Need localized name for child process type.";
  }

  return result_title;
}

// Creates the Mojo service wrapper that will be used to sample the V8 memory
// usage of the browser child process whose unique ID is
// |unique_child_process_id|.
ProcessResourceUsage* CreateProcessResourcesSampler(
    int unique_child_process_id) {
  mojo::PendingRemote<content::mojom::ResourceUsageReporter> usage_reporter;
  content::BrowserChildProcessHost* host =
      content::BrowserChildProcessHost::FromID(unique_child_process_id);
  auto receiver = usage_reporter.InitWithNewPipeAndPassReceiver();
  if (host)
    host->GetHost()->BindReceiver(std::move(receiver));

  return new ProcessResourceUsage(std::move(usage_reporter));
}

bool UsesV8Memory(int process_type) {
  switch (process_type) {
    case content::PROCESS_TYPE_UTILITY:
    case content::PROCESS_TYPE_BROWSER:
    case content::PROCESS_TYPE_RENDERER:
      return true;

    default:
      return false;
  }
}

}  // namespace

gfx::ImageSkia* ChildProcessTask::s_icon_ = nullptr;

ChildProcessTask::ChildProcessTask(const content::ChildProcessData& data,
                                   ProcessSubtype subtype)
    : Task(GetLocalizedTitle(data.name, data.process_type, subtype),
           FetchIcon(IDR_PLUGINS_FAVICON, &s_icon_),
           data.GetProcess().Handle()),
      process_resources_sampler_(CreateProcessResourcesSampler(data.id)),
      unique_child_process_id_(data.id),
      process_type_(data.process_type),
      process_subtype_(subtype),
      uses_v8_memory_(UsesV8Memory(process_type_)) {}

ChildProcessTask::~ChildProcessTask() = default;

void ChildProcessTask::Refresh(const base::TimeDelta& update_interval,
                               int64_t refresh_flags) {
  Task::Refresh(update_interval, refresh_flags);

  if ((refresh_flags & REFRESH_TYPE_V8_MEMORY) == 0)
    return;

  if (!uses_v8_memory_)
    return;

  // The child process resources refresh is performed asynchronously, we will
  // invoke it and record the current values (which might be invalid at the
  // moment. We can safely ignore that and count on future refresh cycles
  // potentially having valid values).
  process_resources_sampler_->Refresh(base::DoNothing());

  v8_memory_allocated_ =
      base::ByteCount(process_resources_sampler_->GetV8MemoryAllocated());
  v8_memory_used_ =
      base::ByteCount(process_resources_sampler_->GetV8MemoryUsed());
}

Task::Type ChildProcessTask::GetType() const {
  // Convert |content::ProcessType| to |task_manager::Task::Type|.
  switch (process_type_) {
    case content::PROCESS_TYPE_PPAPI_PLUGIN_DEPRECATED:
    case content::PROCESS_TYPE_PPAPI_BROKER_DEPRECATED:
      NOTREACHED();
    case content::PROCESS_TYPE_UTILITY:
      return Task::UTILITY;
    case content::PROCESS_TYPE_ZYGOTE:
      return Task::ZYGOTE;
    case content::PROCESS_TYPE_SANDBOX_HELPER:
      return Task::SANDBOX_HELPER;
    case content::PROCESS_TYPE_GPU:
      return Task::GPU;
    case content::PROCESS_TYPE_RENDERER:
      return Task::RENDERER;
    default:
      return Task::UNKNOWN;
  }
}

Task::SubType ChildProcessTask::GetSubType() const {
  // Please consult Task Manager OWNERs when adding a new ProcessSubType.
  switch (process_subtype_) {
    case ChildProcessTask::ProcessSubtype::kSpareRenderProcess:
      return Task::SubType::kSpareRenderer;
#if BUILDFLAG(ENABLE_GLIC)
    case ChildProcessTask::ProcessSubtype::kGlicRenderProcess:
#endif
    case ChildProcessTask::ProcessSubtype::kUnknownRenderProcess:
      return Task::SubType::kUnknownRenderer;
    default:
      return Task::SubType::kNoSubType;
  }
}

int ChildProcessTask::GetChildProcessUniqueID() const {
  return unique_child_process_id_;
}

base::ByteCount ChildProcessTask::GetV8MemoryAllocated() const {
  return v8_memory_allocated_;
}

base::ByteCount ChildProcessTask::GetV8MemoryUsed() const {
  return v8_memory_used_;
}

}  // namespace task_manager
