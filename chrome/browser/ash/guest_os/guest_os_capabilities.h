// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_CAPABILITIES_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_CAPABILITIES_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/capabilities.h"

namespace base {
class FilePath;
}

namespace guest_os {

// This is a safer wrapper
class GuestOsCapabilities : public exo::Capabilities {
 public:
  GuestOsCapabilities();

  ~GuestOsCapabilities() override;

  using BuildCallback =
      base::OnceCallback<void(base::WeakPtr<GuestOsCapabilities>,
                              bool,
                              const base::FilePath& path)>;

  // When |capabilities| are used to build a wayland server, we transfer
  // ownership to Exo. The |callback| will be invoked with the result of that
  // build.
  static void BuildServer(std::unique_ptr<GuestOsCapabilities> capabilities,
                          BuildCallback callback);

  // This method safely removes the server at |path| based on whether
  // |capabilities| is still valid or not. This is useful if you think removing
  // the server might race against exo's shutdown.
  static void MaybeRemoveServer(base::WeakPtr<GuestOsCapabilities> capabilities,
                                const base::FilePath& path);

 private:
  base::WeakPtrFactory<GuestOsCapabilities> weak_factory_;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_CAPABILITIES_H_
