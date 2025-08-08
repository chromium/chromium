// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"

namespace actor_login {
class ActorLoginService;
struct Credential;
}  // namespace actor_login

namespace actor {

class AggregatedJournal;

// Provides tools with functionality implemented by the code invoking the tool.
class ToolDelegate {
 public:
  virtual ~ToolDelegate() = default;

  // Returns the journal so that tools may log information related to their
  // execution.
  virtual AggregatedJournal& GetJournal() = 0;

  // Returns the login service associated with the task.
  virtual actor_login::ActorLoginService& GetActorLoginService() = 0;

  // Prompts the user to select a credential from the list of credentials.
  // The callback is called with the selected credential or with an empty
  // credential if the user closed the prompt without making a selection.
  using CredentialSelectedCallback =
      base::OnceCallback<void(const std::optional<actor_login::Credential>&)>;
  virtual void PromptToSelectCredential(
      const std::vector<actor_login::Credential>& credentials,
      CredentialSelectedCallback callback) = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_
