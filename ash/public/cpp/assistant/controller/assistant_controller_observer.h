// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_CONTROLLER_OBSERVER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_CONTROLLER_OBSERVER_H_

#include <map>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

class GURL;

namespace ash {

namespace assistant {
namespace util {
enum class DeepLinkType;
}  // namespace util
}  // namespace assistant

// A checked observer which receives notification of changes to the
// AssistantController.
class ASH_PUBLIC_EXPORT AssistantControllerObserver
    : public base::CheckedObserver {
 public:
  AssistantControllerObserver(const AssistantControllerObserver&) = delete;
  AssistantControllerObserver& operator=(const AssistantControllerObserver&) =
      delete;

  // Invoked when the AssistantController has been fully constructed.
  virtual void OnAssistantControllerConstructed() {}

  // Invoked when the AssistantController is starting to be destroyed.
  virtual void OnAssistantControllerDestroying() {}

  // Invoked when the Assistant is ready.
  virtual void OnAssistantReady() {}

  // Invoked when Assistant has received a deep link of the specified |type|
  // with the given |params|.
  virtual void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) {}

  // Invoked when the specified |url| is about to be opened by Assistant in a
  // new tab. If |from_server| is true, this event was triggered by a server
  // response. Note that this event immediately precedes |OnUrlOpened|.
  virtual void OnOpeningUrl(const GURL& url,
                            bool in_background,
                            bool from_server) {}

  // Invoked when the specified |url| is opened by Assistant in a new tab. If
  // |from_server| is true, this event was triggered by a server response.
  virtual void OnUrlOpened(const GURL& url, bool from_server) {}

 protected:
  AssistantControllerObserver() = default;
  ~AssistantControllerObserver() override = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_CONTROLLER_OBSERVER_H_
