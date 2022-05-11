// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_CLIENT_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_CLIENT_H_

#include <string>

class GURL;
class PrefRegistrySimple;

namespace content {
class WebContents;
}  // namespace content

// Abstract interface to encapsulate an automated password change (APC) flow.
class ApcClient {
 public:
  // Registers the prefs that are related to automated password change on
  // Desktop.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Static method that acts as a factory function. It is actually implemented
  // |ApcClientImpl|.
  static ApcClient* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  ApcClient(const ApcClient&) = delete;
  ApcClient& operator=(const ApcClient&) = delete;

  // Starts the automated password change flow. Returns true if the flow start
  // was successful.
  virtual bool Start(const GURL& url,
                     const std::string& username,
                     bool skip_login) = 0;

  // Terminates the current APC flow and sets the internal state to make itself
  // available for future calls to run.
  virtual void Stop() = 0;

  // Returns whether a flow is currently running, regardless of whether it is
  // in the onboarding phase or the execution phase.
  virtual bool IsRunning() const = 0;

 protected:
  ApcClient() = default;
  virtual ~ApcClient() = default;
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_CLIENT_H_
