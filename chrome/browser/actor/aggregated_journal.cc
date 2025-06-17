// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/aggregated_journal.h"

#include "base/memory/safe_ref.h"
#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace actor {

namespace {

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
                           base::SafeRef<AggregatedJournal> journal)
      : content::WebContentsUserData<JournalObserver>(*web_contents),
        journal_host_receivers_(web_contents, this),
        journal_(journal) {}

  // actor::mojom::JournalClient methods.
  void AddEntriesToJournal(
      std::vector<mojom::JournalEntryPtr> entries) override {
    journal_->AppendJournalEntries(
        journal_host_receivers_.GetCurrentTargetFrame(), std::move(entries));
  }

  content::RenderFrameHostReceiverSet<mojom::JournalClient>
      journal_host_receivers_;

  base::SafeRef<AggregatedJournal> journal_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(JournalObserver);

}  // namespace

AggregatedJournal::Entry::Entry(const std::string& location,
                                mojom::JournalEntryPtr data_arg)
    : url(location), data(std::move(data_arg)) {}
AggregatedJournal::Entry::~Entry() = default;

AggregatedJournal::AggregatedJournal() : next_trace_id_(base::RandUint64()) {}
AggregatedJournal::~AggregatedJournal() = default;

AggregatedJournal::PendingAsyncEntry::PendingAsyncEntry(
    base::PassKey<AggregatedJournal> pass_key,
    base::SafeRef<AggregatedJournal> journal,
    TaskId task_id,
    uint64_t trace_id,
    std::string_view event_name)
    : pass_key_(pass_key),
      journal_(journal),
      task_id_(task_id),
      trace_id_(trace_id),
      event_name_(event_name) {}

AggregatedJournal::PendingAsyncEntry::~PendingAsyncEntry() {
  if (!terminated_) {
    EndEntry("");
  }
}

void AggregatedJournal::PendingAsyncEntry::EndEntry(std::string_view details) {
  CHECK(!terminated_);
  terminated_ = true;
  ACTOR_LOG() << "End " << event_name_ << ": " << details;
  journal_->AddEndEvent(pass_key_, task_id_, trace_id_, event_name_, details);
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

std::unique_ptr<AggregatedJournal::PendingAsyncEntry>
AggregatedJournal::CreatePendingAsyncEntry(const GURL& url,
                                           TaskId task_id,
                                           std::string_view event_name,
                                           std::string_view details) {
  ACTOR_LOG() << "Begin " << event_name << ": " << details;

  uint64_t trace_id = next_trace_id_++;
  AddEntry(std::make_unique<Entry>(
      url.possibly_invalid_spec(),
      mojom::JournalEntry::New(
          mojom::JournalEntryType::kBegin, task_id.GetUnsafeValue(), trace_id,
          base::Time::Now(), std::string(event_name), std::string(details))));
  return base::WrapUnique(new PendingAsyncEntry(
      base::PassKey<AggregatedJournal>(), weak_ptr_factory_.GetSafeRef(),
      task_id, trace_id, event_name));
}

void AggregatedJournal::Log(const GURL& url,
                            TaskId task_id,
                            std::string_view event_name,
                            std::string_view details) {
  ACTOR_LOG() << event_name << ": " << details;
  AddEntry(std::make_unique<Entry>(
      url.possibly_invalid_spec(),
      mojom::JournalEntry::New(
          mojom::JournalEntryType::kInstant, task_id.GetUnsafeValue(), /*id=*/0,
          base::Time::Now(), std::string(event_name), std::string(details))));
}

void AggregatedJournal::EnsureJournalBound(content::RenderFrameHost& rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(&rfh);
  CHECK(web_contents);
  auto* journal_observer = JournalObserver::FromWebContents(web_contents);
  if (!journal_observer) {
    JournalObserver::CreateForWebContents(web_contents,
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
    content::RenderFrameHost* rfh,
    std::vector<mojom::JournalEntryPtr> entries) {
  std::string location = rfh->GetLastCommittedURL().possibly_invalid_spec();
  for (auto& renderer_entry : entries) {
    AddEntry(std::make_unique<Entry>(location, std::move(renderer_entry)));
  }
}

void AggregatedJournal::AddEndEvent(base::PassKey<AggregatedJournal> pass_key,
                                    TaskId task_id,
                                    uint64_t trace_id,
                                    const std::string& event_name,
                                    std::string_view details) {
  AddEntry(std::make_unique<Entry>(
      std::string(),
      mojom::JournalEntry::New(
          mojom::JournalEntryType::kEnd, task_id.GetUnsafeValue(), trace_id,
          base::Time::Now(), event_name, std::string(details))));
}

void AggregatedJournal::LogScreenshot(const GURL& url,
                                      TaskId task_id,
                                      std::string_view mime_type,
                                      const std::vector<uint8_t>& data) {
  CHECK_EQ(mime_type, "image/jpeg");
  auto entry = std::make_unique<Entry>(
      url.possibly_invalid_spec(),
      mojom::JournalEntry::New(
          mojom::JournalEntryType::kInstant, task_id.GetUnsafeValue(), /*id=*/0,
          base::Time::Now(), "Screenshot", /*details=*/std::string()));
  entry->jpg_screenshot.emplace(data);
  AddEntry(std::move(entry));
}

void AggregatedJournal::AddEntry(std::unique_ptr<Entry> new_entry) {
  for (auto& observer : observers_) {
    observer.WillAddJournalEntry(*new_entry);
  }
  entries_.SaveToBuffer(std::move(new_entry));
}

}  // namespace actor
