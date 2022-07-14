// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_CLIENT_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/callback_helpers.h"

class GURL;
class PrefRegistrySimple;

namespace content {
class WebContents;
}  // namespace content

// Abstract interface to encapsulate an automated password change (APC) flow.
class ApcClient {
 public:
  using ResultCallback = base::OnceCallback<void(bool)>;

  // Registers the prefs that are related to automated password change on
  // Desktop.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Static method that acts as a factory function. It is actually implemented
  // `ApcClientImpl`.
  static ApcClient* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  ApcClient(const ApcClient&) = delete;
  ApcClient& operator=(const ApcClient&) = delete;

  // Starts the automated password change flow at `url` with `username`.
  // Calls `callback` at the termination of the flow with a boolean parameter
  // that indicates whether the credential was changed successfully.
  virtual void Start(const GURL& url,
                     const std::string& username,
                     bool skip_login,
                     ResultCallback callback = base::DoNothing()) = 0;

  // Terminates the current APC flow and sets the internal state to make itself
  // available for future calls to run.
  virtual void Stop(bool success = false) = 0;

  // Returns whether a flow is currently running, regardless of whether it is
  // in the onboarding phase or the execution phase.
  virtual bool IsRunning() const = 0;

  // The two methods below are supposed to be called from the UI to handle
  // granting and revoking consent from outside of APC flows. At the moment,
  // that can only happen in settings. If, at a later point, Autofill Assistant
  // is used outside of password change on Desktop, it may make sense to move
  // these methods out of `ApcClient` into a more global location.

  // Prompts the user to give consent to use Autofill Assistant. Does nothing
  // if consent has either been given already or there is an ongoing APC run in
  // this `WebContents`.
  virtual void PromptForConsent() = 0;

  // Revokes consent to use Autofill Assistant, where `description_grd_ids` are
  // the resource ids of the text on the description labels.
  virtual void RevokeConsent(const std::vector<int>& description_grd_ids) = 0;

 protected:
  ApcClient() = default;
  virtual ~ApcClient() = default;
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_CLIENT_H_
