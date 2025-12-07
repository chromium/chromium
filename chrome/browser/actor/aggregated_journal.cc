// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/aggregated_journal.h"

#include "base/memory/safe_ref.h"
#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace actor {

namespace {

class NonTerminatedJournalEntries
    : public content::DocumentUserData<NonTerminatedJournalEntries> {
 public:
  ~NonTerminatedJournalEntries() override {
    // Terminate any entries that have not been terminated indicating
    // they were ended because of disconnection.
    for (auto& pending_entry : entries_) {
      pending_entry.second->EndEntry(
          JournalDetailsBuilder()
              .Add("end_reason", "Connection Disconnected")
              .Build());
    }
  }

  void TrackEntries(base::PassKey<AggregatedJournal> pass_key,
                    base::SafeRef<AggregatedJournal> journal,
                    const std::vector<mojom::JournalEntryPtr>& entries) {
    for (auto& renderer_entry : entries) {
      if (renderer_entry->type == mojom::JournalEntryType::kBegin) {
        entries_[renderer_entry->event] =
            std::make_unique<AggregatedJournal::PendingAsyncEntry>(
                pass_key, journal, renderer_entry->task_id,
                renderer_entry->event, renderer_entry->track_uuid);
      } else if (renderer_entry->type == mojom::JournalEntryType::kEnd) {
        auto it = entries_.find(renderer_entry->event);
        if (it != entries_.end()) {
          it->second->mark_as_terminated();
          entries_.erase(it);
        }
      }
    }
  }

 private:
  explicit NonTerminatedJournalEntries(content::RenderFrameHost* rfh)
      : DocumentUserData(rfh) {}

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  std::map<std::string, std::unique_ptr<AggregatedJournal::PendingAsyncEntry>>
      entries_;
};

DOCUMENT_USER_DATA_KEY_IMPL(NonTerminatedJournalEntries);

class JournalObserver : public mojom::JournalClient,
                        public content::WebContentsUserData<JournalObserver> {
 public:
  JournalObserver(const JournalObserver&) = delete;
  JournalObserver& operator=(const JournalObserver&) = delete;

  ~JournalObserver() override {}

  void EnsureJournalBound(content::RenderFrameHost& render_frame_host) {
    if (journal_host_receivers_.IsBound(&render_frame_host)) {
      return;
    }
    mojo::PendingAssociatedRemote<actor::mojom::JournalClient> client;
    journal_host_receivers_.Bind(&render_frame_host,
                                 client.InitWithNewEndpointAndPassReceiver());
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> renderer;
    render_frame_host.GetRemoteAssociatedInterfaces()->GetInterface(&renderer);
    renderer->StartActorJournal(std::move(client));
  }

 private:
  friend class content::WebContentsUserData<JournalObserver>;

  explicit JournalObserver(content::WebContents* web_contents,
                           base::PassKey<AggregatedJournal> pass_key,
                           base::SafeRef<AggregatedJournal> journal)
      : content::WebContentsUserData<JournalObserver>(*web_contents),
        journal_host_receivers_(web_contents, this),
        pass_key_(pass_key),
        journal_(journal) {}

  // actor::mojom::JournalClient methods.
  void AddEntriesToJournal(
      std::vector<mojom::JournalEntryPtr> entries) override {
    NonTerminatedJournalEntries::GetOrCreateForCurrentDocument(
        journal_host_receivers_.GetCurrentTargetFrame())
        ->TrackEntries(pass_key_, journal_, entries);
    journal_->AppendJournalEntries(
        *journal_host_receivers_.GetCurrentTargetFrame(), std::move(entries));
  }

  content::RenderFrameHostReceiverSet<mojom::JournalClient>
      journal_host_receivers_;

  base::PassKey<AggregatedJournal> pass_key_;
  base::SafeRef<AggregatedJournal> journal_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(JournalObserver);

}  // namespace

AggregatedJournal::Entry::Entry(const std::string& location,
                                mojom::JournalEntryPtr data_arg)
    : url(location), data(std::move(data_arg)) {}
AggregatedJournal::Entry::~Entry() = default;

AggregatedJournal::AggregatedJournal() = default;
AggregatedJournal::~AggregatedJournal() = default;

AggregatedJournal::PendingAsyncEntry::PendingAsyncEntry(
    base::PassKey<AggregatedJournal> pass_key,
    base::SafeRef<AggregatedJournal> journal,
    TaskId task_id,
    std::string_view event_name,
    uint64_t track_uuid)
    : pass_key_(pass_key),
      journal_(journal),
      task_id_(task_id),
      event_name_(event_name),
      begin_time_(base::TimeTicks::Now()),
      track_uuid_(track_uuid) {}

AggregatedJournal::PendingAsyncEntry::~PendingAsyncEntry() {
  if (!terminated_) {
    EndEntry({});
  }
}

void AggregatedJournal::PendingAsyncEntry::EndEntry(
    std::vector<mojom::JournalDetailsPtr> details) {
  CHECK(!terminated_);
  terminated_ = true;
  ACTOR_LOG() << "End " << event_name_ << ": " << details;
  journal_->AddEndEvent(pass_key_, task_id_, event_name_, track_uuid_,
                        std::move(details));
}

AggregatedJournal& AggregatedJournal::PendingAsyncEntry::GetJournal() {
  return *journal_;
}

TaskId AggregatedJournal::PendingAsyncEntry::GetTaskId() {
  return task_id_;
}

base::SafeRef<AggregatedJournal> AggregatedJournal::GetSafeRef() {
  return weak_ptr_factory_.GetSafeRef();
}

uint64_t AggregatedJournal::AllocateDynamicTrackUUID() {
  static uint64_t next_track_id = 1000;
  return ++next_track_id;
}

std::unique_ptr<AggregatedJournal::PendingAsyncEntry>
AggregatedJournal::CreatePendingAsyncEntry(
    const GURL& url,
    TaskId task_id,
    uint64_t track_uuid,
    std::string_view event_name,
    std::vector<mojom::JournalDetailsPtr> details) {
  ACTOR_LOG() << "Begin " << event_name << ": " << details;

  AddEntry(std::make_unique<Entry>(
      url.possibly_invalid_spec(),
      mojom::JournalEntry::New(mojom::JournalEntryType::kBegin, task_id,
                               base::Time::Now(), std::string(event_name),
                               track_uuid, std::move(details))));
  return base::WrapUnique(new PendingAsyncEntry(
      base::PassKey<AggregatedJournal>(), weak_ptr_factory_.GetSafeRef(),
      task_id, event_name, track_uuid));
}

void AggregatedJournal::Log(const GURL& url,
                            TaskId task_id,
                            std::string_view event_name,
                            std::vector<mojom::JournalDetailsPtr> details) {
  Log(url, task_id, MakeBrowserTrackUUID(task_id), event_name,
      std::move(details));
}

void AggregatedJournal::Log(const GURL& url,
                            TaskId task_id,
                            uint64_t track_uuid,
                            std::string_view event_name,
                            std::vector<mojom::JournalDetailsPtr> details) {
  ACTOR_LOG() << event_name << ": " << details;
  AddEntry(std::make_unique<Entry>(
      url.possibly_invalid_spec(),
      mojom::JournalEntry::New(mojom::JournalEntryType::kInstant, task_id,
                               base::Time::Now(), std::string(event_name),
                               track_uuid, std::move(details))));
}

void AggregatedJournal::EnsureJournalBound(content::RenderFrameHost& rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(&rfh);
  CHECK(web_contents);
  auto* journal_observer = JournalObserver::FromWebContents(web_contents);
  if (!journal_observer) {
    JournalObserver::CreateForWebContents(web_contents,
                                          base::PassKey<AggregatedJournal>(),
                                          weak_ptr_factory_.GetSafeRef());
    journal_observer = JournalObserver::FromWebContents(web_contents);
  }

  journal_observer->EnsureJournalBound(rfh);
}

void AggregatedJournal::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AggregatedJournal::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AggregatedJournal::AppendJournalEntries(
    content::RenderFrameHost& rfh,
    std::vector<mojom::JournalEntryPtr> entries) {
  std::string location = rfh.GetLastCommittedURL().possibly_invalid_spec();
  for (auto& renderer_entry : entries) {
    AddEntry(std::make_unique<Entry>(location, std::move(renderer_entry)));
  }
}

void AggregatedJournal::AddEndEvent(
    base::PassKey<AggregatedJournal> pass_key,
    TaskId task_id,
    const std::string& event_name,
    uint64_t track_uuid,
    std::vector<mojom::JournalDetailsPtr> details) {
  AddEntry(std::make_unique<Entry>(
      std::string(),
      mojom::JournalEntry::New(mojom::JournalEntryType::kEnd, task_id,
                               base::Time::Now(), event_name, track_uuid,
                               std::move(details))));
}

void AggregatedJournal::LogScreenshot(const GURL& url,
                                      TaskId task_id,
                                      std::string_view mime_type,
                                      base::span<const uint8_t> data) {
  auto entry = std::make_unique<Entry>(
      url.possibly_invalid_spec(),
      mojom::JournalEntry::New(
          mojom::JournalEntryType::kInstant, task_id, base::Time::Now(),
          "Screenshot", MakeBrowserTrackUUID(task_id),
          /*details=*/std::vector<mojom::JournalDetailsPtr>()));
  entry->screenshot.emplace(data.begin(), data.end());
  AddEntry(std::move(entry));
}

void AggregatedJournal::LogAnnotatedPageContent(
    const GURL& url,
    TaskId task_id,
    base::span<const uint8_t> data) {
  auto entry = std::make_unique<Entry>(
      url.possibly_invalid_spec(),
      mojom::JournalEntry::New(
          mojom::JournalEntryType::kInstant, task_id, base::Time::Now(),
          "PageContext", MakeBrowserTrackUUID(task_id),
          /*details=*/std::vector<mojom::JournalDetailsPtr>()));
  entry->annotated_page_content.emplace(data.begin(), data.end());
  AddEntry(std::move(entry));
}

void AggregatedJournal::AddEntry(std::unique_ptr<Entry> new_entry) {
  for (auto& observer : observers_) {
    observer.WillAddJournalEntry(*new_entry);
  }
  entries_.SaveToBuffer(std::move(new_entry));
}

}  // namespace actor
