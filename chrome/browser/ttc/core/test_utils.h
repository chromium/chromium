// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TEST_UTILS_H_
#define CHROME_BROWSER_TTC_CORE_TEST_UTILS_H_

#include <string>

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/ttc/app/public/conversation.h"
#include "components/ttc/app/public/session_journal.h"
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
  MOCK_METHOD(void, SetMicMuted, (bool), (override));
  MOCK_METHOD(bool, IsMicMuted, (), (const, override));
  MOCK_METHOD(SessionJournal&, GetJournal, (), (override));
};

// Runs the message loop for a short amount of time. Useful to check something
// doesn't happen but, since it can't prove the thing won't happen later, use
// this only when there's no event to wait on.
void TinyWait();

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TEST_UTILS_H_
