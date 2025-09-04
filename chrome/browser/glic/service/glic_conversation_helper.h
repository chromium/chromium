// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_CONVERSATION_HELPER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_CONVERSATION_HELPER_H_

#include "base/callback_list.h"
#include "base/uuid.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace glic {

// A type alias for the Glic conversation identifier.
using ConversationId = base::Uuid;

// Attaches a ConversationId to a TabInterface. An instance of this class is
// created by and owned by TabFeatures.
class GlicConversationHelper {
 public:
  DECLARE_USER_DATA(GlicConversationHelper);

  static GlicConversationHelper* From(tabs::TabInterface* tab);

  explicit GlicConversationHelper(tabs::TabInterface* tab);
  ~GlicConversationHelper();

  std::optional<ConversationId> GetConversationId() const {
    return conversation_id_;
  }
  void SetConversationId(const ConversationId& conversation_id) {
    conversation_id_ = conversation_id;
  }

  base::CallbackListSubscription SubscribeToDestruction(
      base::RepeatingCallback<void(tabs::TabInterface*, const ConversationId&)>
          callback);

 private:
  std::optional<ConversationId> conversation_id_;
  raw_ptr<tabs::TabInterface> tab_;
  ui::ScopedUnownedUserData<GlicConversationHelper> scoped_unowned_user_data_;
  base::RepeatingCallbackList<void(tabs::TabInterface*, const ConversationId&)>
      on_destroy_callback_list_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_CONVERSATION_HELPER_H_
