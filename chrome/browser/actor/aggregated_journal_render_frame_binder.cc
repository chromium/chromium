// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/aggregated_journal_render_frame_binder.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/safe_ref.h"
#include "base/types/pass_key.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/actor/core/journal_details_builder.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents.h"
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

  ~JournalObserver() override = default;

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
        journal_host_receivers_.GetCurrentTargetFrame()->GetLastCommittedURL(),
        std::move(entries));
  }

  content::RenderFrameHostReceiverSet<mojom::JournalClient>
      journal_host_receivers_;

  base::PassKey<AggregatedJournal> pass_key_;
  base::SafeRef<AggregatedJournal> journal_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(JournalObserver);

}  // namespace

// static
void AggregatedJournalRenderFrameBinder::EnsureBound(
    AggregatedJournal& journal,
    content::RenderFrameHost& rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(&rfh);
  CHECK(web_contents);
  auto* journal_observer = JournalObserver::FromWebContents(web_contents);
  if (!journal_observer) {
    JournalObserver::CreateForWebContents(
        web_contents,
        AggregatedJournal::CreatePassKey(
            base::PassKey<AggregatedJournalRenderFrameBinder>()),
        journal.GetSafeRef());
    journal_observer = JournalObserver::FromWebContents(web_contents);
  }

  journal_observer->EnsureJournalBound(rfh);
}

}  // namespace actor
