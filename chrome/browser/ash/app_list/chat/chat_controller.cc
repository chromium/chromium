// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/chat/chat_controller.h"

#include "chrome/browser/profiles/profile.h"

// TODO(b/268140386): This file is a work in progress.

namespace app_list {

ChatController::ChatController(Profile* profile) : profile_(profile) {
  DCHECK(profile_);
}

ChatController::~ChatController() = default;

}  // namespace app_list
