// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SERVER_JOURNAL_EVENT_H_
#define CHROME_BROWSER_TTC_CORE_SERVER_JOURNAL_EVENT_H_

#include <stdint.h>

#include <string>
#include <vector>

namespace ttc {

// Event recorded in the session journal from the server.
struct ServerJournalEvent {
  enum class Type {
    kUnspecified,
    kInstant,
    kAsyncBegin,
    kAsyncEnd,
  };

  struct Details {
    std::string key;
    std::string value;
  };

  ServerJournalEvent();
  ServerJournalEvent(const ServerJournalEvent&);
  ServerJournalEvent& operator=(const ServerJournalEvent&);
  ServerJournalEvent(ServerJournalEvent&&);
  ServerJournalEvent& operator=(ServerJournalEvent&&);
  ~ServerJournalEvent();

  int64_t timestamp_us = 0;
  Type type = Type::kUnspecified;
  std::string name;
  std::vector<Details> details;
  int32_t async_event_id = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SERVER_JOURNAL_EVENT_H_
