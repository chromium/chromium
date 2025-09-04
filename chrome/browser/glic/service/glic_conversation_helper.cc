// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_conversation_helper.h"

namespace glic {

DEFINE_USER_DATA(GlicConversationHelper);

GlicConversationHelper* GlicConversationHelper::From(tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

GlicConversationHelper::GlicConversationHelper(tabs::TabInterface* tab)
    : tab_(tab),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}

GlicConversationHelper::~GlicConversationHelper() {
  if (!conversation_id_.has_value()) {
    return;
  }
  CHECK(tab_);
  on_destroy_callback_list_.Notify(tab_, conversation_id_.value());
}

base::CallbackListSubscription GlicConversationHelper::SubscribeToDestruction(
    base::RepeatingCallback<void(tabs::TabInterface*, const ConversationId&)>
        callback) {
  return on_destroy_callback_list_.Add(std::move(callback));
}

}  // namespace glic
