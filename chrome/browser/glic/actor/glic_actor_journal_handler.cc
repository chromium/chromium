// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_journal_handler.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/aggregated_journal_file_serializer.h"
#include "chrome/browser/actor/aggregated_journal_in_memory_serializer.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "components/feedback/content/content_tracing_manager.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace glic {
namespace {
constexpr std::string_view kGlicActorJournalLog = "glic-actor-journal";
}

GlicActorJournalHandler::GlicActorJournalHandler(Profile* profile)
    : actor_keyed_service_(actor::ActorKeyedService::Get(profile)) {
  base::FilePath path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kGlicActorJournalLog);
  if (!path.empty()) {
    GetUniquePath(path,
                  base::BindOnce(&GlicActorJournalHandler::OnUniquePathReceived,
                                 base::Unretained(this)));
  }
}

GlicActorJournalHandler::~GlicActorJournalHandler() = default;

void GlicActorJournalHandler::LogBeginAsyncEvent(uint64_t event_async_id,
                                                 int32_t task_id,
                                                 const std::string& event,
                                                 const std::string& details) {
  // If there is a matching ID make sure it terminates before the new event
  // is created.
  auto it = active_journal_events_.find(event_async_id);
  if (it != active_journal_events_.end()) {
    active_journal_events_.erase(it);
  }

  auto actor_task_id = actor::TaskId(task_id);
  active_journal_events_[event_async_id] =
      actor_keyed_service_->GetJournal().CreatePendingAsyncEntry(
          /*url=*/GURL::EmptyGURL(), actor_task_id,
          actor::MakeFrontEndTrackUUID(actor_task_id), event,
          actor::JournalDetailsBuilder().Add("begin_details", details).Build());
}

void GlicActorJournalHandler::LogEndAsyncEvent(uint64_t event_async_id,
                                               const std::string& details) {
  auto it = active_journal_events_.find(event_async_id);
  if (it != active_journal_events_.end()) {
    it->second->EndEntry(
        actor::JournalDetailsBuilder().Add("end_details", details).Build());

    if (!it->second->GetTaskId().is_null()) {
      // Log a histogram for each async event.
      std::string histogram_name;
      // The event name may have whitespaces and that won't work as a
      // histogram name.
      base::RemoveChars(it->second->event_name(), " ", &histogram_name);

      base::UmaHistogramLongTimes100(
          "Glic.Actor.JournalEvent." + histogram_name,
          base::TimeTicks::Now() - it->second->begin_time());
    }

    active_journal_events_.erase(it);
  }
}

void GlicActorJournalHandler::LogInstantEvent(int32_t task_id,
                                              const std::string& event,
                                              const std::string& details) {
  auto actor_task_id = actor::TaskId(task_id);
  actor_keyed_service_->GetJournal().Log(
      /*url=*/GURL::EmptyGURL(), actor_task_id,
      actor::MakeFrontEndTrackUUID(actor_task_id), event,
      actor::JournalDetailsBuilder().Add("details", details).Build());
}

void GlicActorJournalHandler::Clear() {
  if (journal_serializer_) {
    journal_serializer_->Clear();
  }
}

void GlicActorJournalHandler::Snapshot(
    bool clear_journal,
    glic::mojom::WebClientHandler::JournalSnapshotCallback callback) {
  if (!journal_serializer_) {
    std::move(callback).Run(glic::mojom::Journal::New());
    return;
  }
  std::move(callback).Run(
      glic::mojom::Journal::New(journal_serializer_->Snapshot()));
  if (clear_journal) {
    journal_serializer_->Clear();
  }
}

std::vector<uint8_t> GlicActorJournalHandler::GetSnapshot(bool clear_journal) {
  std::vector<uint8_t> result_buffer;
  if (journal_serializer_) {
    result_buffer = journal_serializer_->Snapshot();
    if (clear_journal) {
      journal_serializer_->Clear();
    }
  }
  return result_buffer;
}

void GlicActorJournalHandler::Start(uint64_t max_bytes,
                                    bool capture_screenshots) {
  journal_serializer_ =
      std::make_unique<actor::AggregatedJournalInMemorySerializer>(
          actor_keyed_service_->GetJournal(), max_bytes);
  journal_serializer_->Init();
}

void GlicActorJournalHandler::Stop() {
  journal_serializer_.reset();
}

void GlicActorJournalHandler::RecordFeedback(bool positive,
                                             const std::string& reason) {
  if (base::FeatureList::IsEnabled(features::kGlicRecordActorJournal) &&
      !positive) {
    SendResponseFeedback(reason);
  }
}

void GlicActorJournalHandler::SendResponseFeedback(const std::string& reason) {
  base::WeakPtr<feedback::FeedbackUploader> uploader =
      feedback::FeedbackUploaderFactoryChrome::GetForBrowserContext(
          actor_keyed_service_->GetProfile())
          ->AsWeakPtr();
  scoped_refptr<::feedback::FeedbackData> feedback_data =
      base::MakeRefCounted<feedback::FeedbackData>(
          std::move(uploader), ContentTracingManager::Get());
  auto journal = GetSnapshot(false);

  // TODO(b/430054430): Fetch and include system data to the feedback.
  feedback_data->set_description(
      reason + " - " + base::Uuid::GenerateRandomV4().AsLowercaseString());
  feedback_data->set_product_id(
      features::kGlicRecordActorJournalFeedbackProductId.Get());
  feedback_data->set_category_tag(
      features::kGlicRecordActorJournalFeedbackCategoryTag.Get());
  feedback_data->set_is_offensive_or_unsafe(false);
  feedback_data->AddFile("actor-journal", journal);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(actor_keyed_service_->GetProfile());
  if (identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    feedback_data->set_user_email(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .email);
  }

// NEEDS_ANDROID_IMPL: ChromeSystemLogsFetcher
#if BUILDFLAG(IS_ANDROID)
  feedback_data->CompressSystemInfo();
  feedback_data->OnFeedbackPageDataComplete();
#else
  system_logs::BuildChromeSystemLogsFetcher(actor_keyed_service_->GetProfile(),
                                            /*scrub_data=*/false)
      ->Fetch(base::BindOnce(
          [](scoped_refptr<::feedback::FeedbackData> feedback_data,
             std::unique_ptr<system_logs::SystemLogsResponse>
                 system_logs_response) {
            if (system_logs_response) {
              feedback_data->AddLogs(*system_logs_response);
            }
            feedback_data->CompressSystemInfo();
            feedback_data->OnFeedbackPageDataComplete();
          },
          std::move(feedback_data)));
#endif
}

void GlicActorJournalHandler::GetUniquePath(
    const base::FilePath& file_path,
    base::OnceCallback<void(const base::FilePath&)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&base::GetUniquePathWithSuffixFormat, file_path,
                     base::cstring_view("_%d")),
      std::move(callback));
}

void GlicActorJournalHandler::OnUniquePathReceived(
    const base::FilePath& unique_path) {
  LOG(ERROR) << "Glic Journal: " << unique_path;
  file_journal_serializer_ =
      std::make_unique<actor::AggregatedJournalFileSerializer>(
          actor_keyed_service_->GetJournal());
  file_journal_serializer_->Init(
      unique_path, base::BindOnce(&GlicActorJournalHandler::FileInitDone,
                                  base::Unretained(this)));
}

void GlicActorJournalHandler::FileInitDone(bool success) {
  if (!success) {
    file_journal_serializer_.reset();
  }
}

}  // namespace glic
