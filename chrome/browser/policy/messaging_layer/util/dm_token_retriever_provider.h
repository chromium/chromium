// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_DM_TOKEN_RETRIEVER_PROVIDER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_DM_TOKEN_RETRIEVER_PROVIDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/reporting/client/dm_token_retriever.h"
#include "components/reporting/client/report_queue_configuration.h"

namespace reporting {

// |DMTokenRetrieverProvider| is used to retrieve an appropriate
// |DMTokenRetriever| for a given event type. These retrievers can later be used
// to retrieve DM tokens of certain kinds (user/device, for instance).
class DMTokenRetrieverProvider {
 public:
  using DMTokenRetrieverCallback =
      base::RepeatingCallback<std::unique_ptr<DMTokenRetriever>(EventType)>;

  DMTokenRetrieverProvider() = default;
  ~DMTokenRetrieverProvider() = default;
  DMTokenRetrieverProvider(const DMTokenRetrieverProvider& other) = delete;
  DMTokenRetrieverProvider& operator=(const DMTokenRetrieverProvider& other) =
      delete;

  // Gets an appropriate DM token retriever for reporting purposes.
  std::unique_ptr<DMTokenRetriever> GetDMTokenRetrieverForEventType(
      EventType event_type);

  // Test hook for stubbing the DM token retriever through a callback. When set,
  // this callback is run and the resulting retriever is always returned.
  static void SetDMTokenRetrieverCallbackForTesting(
      DMTokenRetrieverCallback dm_token_retriever_cb);

 private:
  static DMTokenRetrieverCallback* GetTestDMTokenRetrieverCallback();
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_DM_TOKEN_RETRIEVER_PROVIDER_H_
