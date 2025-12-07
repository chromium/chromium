// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_PUBLIC_SHARED_CRD_SESSION_PROVIDER_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_PUBLIC_SHARED_CRD_SESSION_PROVIDER_H_

#include <memory>

#include "chrome/browser/ash/policy/remote_commands/crd/crd_admin_session_controller.h"
#include "components/prefs/pref_service.h"

namespace policy {

class SharedCrdSession;

// Provider for a `SharedCrdSession`. Used to provide the `SharedCrdSessionImpl`
// inside the non-public folder. When used, a reference of the provider should
// be owned by the calling class so the crd session is not destroyed early.
class SharedCrdSessionProvider {
 public:
  explicit SharedCrdSessionProvider(PrefService* local_state);

  SharedCrdSessionProvider(const SharedCrdSessionProvider&) = delete;
  SharedCrdSessionProvider& operator=(const SharedCrdSessionProvider&) = delete;

  ~SharedCrdSessionProvider();

  std::unique_ptr<SharedCrdSession> GetCrdSession();

 private:
  // The `CrdAdminSessionController` contains an implementation  of the
  // `StartCrdSessionJobDelegate`. The controller is owned by the provider to
  // ensure the delegate is not destroyed after the call to GetCrdSession is
  // finished. The delegate needs to stay alive as long as the callee needs it
  // and thus the callee should keep a owned instance of the provider.
  std::unique_ptr<CrdAdminSessionController> crd_controller_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_PUBLIC_SHARED_CRD_SESSION_PROVIDER_H_
