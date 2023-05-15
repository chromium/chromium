// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SECURITY_DELEGATE_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SECURITY_DELEGATE_H_

#include <memory>

#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/security_delegate.h"

namespace exo {
class WaylandServerHandle;
}

namespace guest_os {

// This is a safer wrapper
class GuestOsSecurityDelegate : public exo::SecurityDelegate {
 public:
  GuestOsSecurityDelegate();

  ~GuestOsSecurityDelegate() override;

  // When |security_delegate| is used to build a wayland server, we transfer
  // ownership to Exo. The |callback| will be invoked with the result of that
  // build.
  static void MakeServerWithFd(
      std::unique_ptr<GuestOsSecurityDelegate> security_delegate,
      base::ScopedFD fd,
      base::OnceCallback<void(base::WeakPtr<GuestOsSecurityDelegate>,
                              std::unique_ptr<exo::WaylandServerHandle>)>
          callback);

 private:
  base::WeakPtrFactory<GuestOsSecurityDelegate> weak_factory_;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SECURITY_DELEGATE_H_
