// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/public/make_conversation.h"

#include "chrome/browser/ttc/app/conversation_impl.h"

namespace ttc {

std::unique_ptr<Conversation> MakeConversationImpl(Profile* profile) {
  return std::make_unique<ConversationImpl>(profile);
}

}  // namespace ttc
