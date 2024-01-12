// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_CHAT_CHAT_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_LIST_CHAT_CHAT_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

class Profile;

// TODO(b/268140386): This file is a work in progress.

namespace app_list {

class ChatController {
 public:
  explicit ChatController(Profile* profile);
  ~ChatController();

  ChatController(const ChatController&) = delete;
  ChatController& operator=(const ChatController&) = delete;

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_CHAT_CHAT_CONTROLLER_H_
