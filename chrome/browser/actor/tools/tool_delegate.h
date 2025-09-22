// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "chrome/common/actor_webui.mojom.h"
#include "url/gurl.h"

class Profile;

namespace actor_login {
class ActorLoginService;
struct Credential;
}  // namespace actor_login

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace gfx {
class Image;
}  // namespace gfx

namespace actor {

class AggregatedJournal;

// Provides tools with functionality implemented by the code invoking the tool.
class ToolDelegate {
 public:
  virtual ~ToolDelegate() = default;

  // Returns the profile in which the task is running.
  virtual Profile& GetProfile() = 0;

  // Returns the journal so that tools may log information related to their
  // execution.
  virtual AggregatedJournal& GetJournal() = 0;

  // Returns the login service associated with the task.
  virtual actor_login::ActorLoginService& GetActorLoginService() = 0;

  // Returns the favicon service for the profile associated with the task.
  virtual favicon::FaviconService* GetFaviconService() = 0;

  // Prompts the user to select a credential from the list of credentials, and
  // with optional icons for each site or app that is associated with the
  // credential.
  // The callback is called with the selected credential or with an empty
  // credential if the user closed the prompt without making a selection.
  using CredentialSelectedCallback = base::OnceCallback<void(
      webui::mojom::SelectCredentialDialogResponsePtr response)>;
  virtual void PromptToSelectCredential(
      const std::vector<actor_login::Credential>& credentials,
      const base::flat_map<std::string, gfx::Image>& icons,
      CredentialSelectedCallback callback) = 0;

  // Sets / gets the credential that the user has chosen to allow the
  // actor to use. The selected credential can be used for multi-step login
  // within the same task.
  virtual void SetUserSelectedCredential(
      const actor_login::Credential& credential) = 0;
  virtual const std::optional<actor_login::Credential>
  GetUserSelectedCredential(const url::Origin& request_origin) const = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_
