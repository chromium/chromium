// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONVERSATION_STARTER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONVERSATION_STARTER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {

// Models an immutable conversation starter.
class ASH_PUBLIC_EXPORT ConversationStarter {
 public:
  // Enumeration of possible permissions which a conversation starter may
  // require in order to be presented to the user. Note that |kUnknown| is used
  // to specify permissions that were requested but not recognized by the client
  // which indicates that it is unsafe to show the associated starter.
  enum Permission : uint32_t { kUnknown = 1u, kRelatedInfo = 2u };

  ConversationStarter(const std::string& label,
                      const absl::optional<GURL>& action_url,
                      const absl::optional<GURL>& icon_url,
                      uint32_t required_permissions);
  ConversationStarter(const ConversationStarter& copy);
  ~ConversationStarter();

  // Whether or not this conversation starter requires |permission|.
  bool RequiresPermission(Permission permission) const;

  const std::string& label() const { return label_; }
  const absl::optional<GURL>& action_url() const { return action_url_; }
  const absl::optional<GURL>& icon_url() const { return icon_url_; }
  uint32_t required_permissions() const { return required_permissions_; }

 private:
  std::string label_;
  absl::optional<GURL> action_url_;
  absl::optional<GURL> icon_url_;
  uint32_t required_permissions_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONVERSATION_STARTER_H_
