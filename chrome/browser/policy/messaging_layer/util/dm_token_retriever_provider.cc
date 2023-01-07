// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/dm_token_retriever_provider.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/policy/messaging_layer/util/user_dm_token_retriever.h"
#include "components/reporting/client/dm_token_retriever.h"
#include "components/reporting/client/empty_dm_token_retriever.h"
#include "components/reporting/client/report_queue_configuration.h"

namespace reporting {

// static
DMTokenRetrieverProvider::DMTokenRetrieverCallback*
DMTokenRetrieverProvider::GetTestDMTokenRetrieverCallback() {
  static base::NoDestructor<DMTokenRetrieverProvider::DMTokenRetrieverCallback>
      dm_token_retriever_cb{
          DMTokenRetrieverProvider::DMTokenRetrieverCallback()};
  return dm_token_retriever_cb.get();
}

// Gets an appropriate DM token retriever for a given event type. We return:
// - a |UserDMTokenRetriever| for events that require user DM tokens
// - a |EmptyDMTokenRetriever| for all other events since we fallback to device
// DM tokens which are appended by default during event uploads.
std::unique_ptr<DMTokenRetriever>
DMTokenRetrieverProvider::GetDMTokenRetrieverForEventType(
    EventType event_type) {
  // Run test callback and return DM token retriever if set
  if (*GetTestDMTokenRetrieverCallback()) {
    return (*GetTestDMTokenRetrieverCallback()).Run(event_type);
  }

  if (event_type == EventType::kUser) {
    return UserDMTokenRetriever::Create();
  }

  return std::make_unique<EmptyDMTokenRetriever>();
}

void DMTokenRetrieverProvider::SetDMTokenRetrieverCallbackForTesting(
    DMTokenRetrieverCallback dm_token_retriever_cb) {
  *DMTokenRetrieverProvider::GetTestDMTokenRetrieverCallback() =
      std::move(dm_token_retriever_cb);
}

}  // namespace reporting
