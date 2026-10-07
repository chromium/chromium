// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TEST_UTILS_H_
#define CHROME_BROWSER_TTC_CORE_TEST_UTILS_H_

#include <stdint.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/ttc/app/public/tool_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace ttc {

class MockConversation : public Conversation {
 public:
  MockConversation();
  ~MockConversation() override;

  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, SendTextInput, (const std::string&), (override));
  MOCK_METHOD(void,
              SendContextUpdate,
              (const GURL&,
               const std::string&,
               const optimization_guide::proto::AnnotatedPageContent&),
              (override));
  MOCK_METHOD(void, OnPageContextInvalidated, (), (override));
};

// Records the entries added to an actor journal while it's alive.
class ActorJournalRecorder : public actor::AggregatedJournal::Observer {
 public:
  struct Entry {
    Entry();
    Entry(const Entry&);
    Entry& operator=(const Entry&);
    ~Entry();

    actor::mojom::JournalEntryType type =
        actor::mojom::JournalEntryType::kInstant;
    std::string event;
    actor::TaskId task_id;
    uint64_t track_uuid = 0;
    std::map<std::string, std::string> details;
  };

  explicit ActorJournalRecorder(actor::AggregatedJournal& journal);
  ActorJournalRecorder(const ActorJournalRecorder&) = delete;
  ActorJournalRecorder& operator=(const ActorJournalRecorder&) = delete;
  ~ActorJournalRecorder() override;

  // Returns the recorded entries for `event`, in the order they were added.
  std::vector<Entry> GetEntries(std::string_view event) const;

  // actor::AggregatedJournal::Observer:
  void WillAddJournalEntry(
      const actor::AggregatedJournal::Entry& entry) override;

 private:
  std::vector<Entry> entries_;
  base::ScopedObservation<actor::AggregatedJournal,
                          actor::AggregatedJournal::Observer>
      observation_{this};
};

// Runs the message loop for a short amount of time. Useful to check something
// doesn't happen but, since it can't prove the thing won't happen later, use
// this only when there's no event to wait on.
void TinyWait();

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TEST_UTILS_H_
