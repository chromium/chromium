// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_SAFETY_LIST_MANAGER_H_
#define CHROME_BROWSER_ACTOR_SAFETY_LIST_MANAGER_H_

#include <string_view>

#include "chrome/browser/actor/safety_list.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace actor {

class SafetyListManager {
 public:
  ~SafetyListManager();

  SafetyListManager(const SafetyListManager&) = delete;
  SafetyListManager& operator=(const SafetyListManager&) = delete;
  SafetyListManager(SafetyListManager&&) = default;
  SafetyListManager& operator=(SafetyListManager&&) = default;

  static SafetyListManager* GetInstance();
  static SafetyListManager CreateForTesting();

  const SafetyList& get_allowed_list() const { return allowed_; }
  const SafetyList& get_blocked_list() const { return blocked_; }

  void ParseSafetyLists(std::string_view json);

 private:
  // For singleton pattern.
  friend class base::NoDestructor<SafetyListManager>;
  SafetyListManager();

  struct ParseStatus {
    SafetyListParseResult allowed_result;
    SafetyListParseResult blocked_result;
  };

  ParseStatus ParseSafetyListsInternal(std::string_view json_string);

  // TODO(crbug.com/453660392): Add hashmap with JSON key -> SafetyList pairing.
  SafetyList allowed_;
  SafetyList blocked_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_SAFETY_LIST_MANAGER_H_
