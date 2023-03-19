// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SECURITY_DELEGATE_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SECURITY_DELEGATE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/security_delegate.h"

namespace base {
class FilePath;
}

namespace guest_os {

// This is a safer wrapper
class GuestOsSecurityDelegate : public exo::SecurityDelegate {
 public:
  GuestOsSecurityDelegate();

  ~GuestOsSecurityDelegate() override;

  using BuildCallback =
      base::OnceCallback<void(base::WeakPtr<GuestOsSecurityDelegate>,
                              bool,
                              const base::FilePath& path)>;

  // When |security_delegate| is used to build a wayland server, we transfer
  // ownership to Exo. The |callback| will be invoked with the result of that
  // build.
  static void BuildServer(
      std::unique_ptr<GuestOsSecurityDelegate> security_delegate,
      BuildCallback callback);

  // This method safely removes the server at |path| based on whether
  // |security_delegate| is still valid or not. This is useful if you think
  // removing the server might race against exo's shutdown.
  static void MaybeRemoveServer(
      base::WeakPtr<GuestOsSecurityDelegate> security_delegate,
      const base::FilePath& path);

  // exo::SecurityDelegate overrides:
  std::string GetSecurityContext() const override;

 private:
  base::WeakPtrFactory<GuestOsSecurityDelegate> weak_factory_;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SECURITY_DELEGATE_H_
