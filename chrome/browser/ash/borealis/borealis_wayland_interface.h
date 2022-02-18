// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_WAYLAND_INTERFACE_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_WAYLAND_INTERFACE_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/borealis_features.h"

class Profile;

namespace borealis {

class BorealisCapabilities;

// This class tracks/owns the necessary information to allow borealis to deal
// with exo. Primarily this means the BorealisCapabilities (which define what
// exo does specially for borealis) and the server's path (which we pass to
// concierge).
//
// This class lazily requests server creation, so that we don't create a server
// unless we actually need it. If the server has been created, this class will
// clean it up in its own destruction (which is tied to the user's session). In
// practice this means that multiple invocations of borealis will all use the
// same server, rather than creating and destroying for each invocation.
class BorealisWaylandInterface {
 public:
  using CapabilityCallback =
      base::OnceCallback<void(BorealisCapabilities*, const base::FilePath&)>;

  explicit BorealisWaylandInterface(Profile* profile);
  ~BorealisWaylandInterface();

  // Invokes the callback with the handle to the wayland server once one is
  // available.
  void GetWaylandServer(CapabilityCallback callback);

 private:
  void OnAllowednessChecked(CapabilityCallback callback,
                            BorealisFeatures::AllowStatus allowed);

  // Called by Exo when the GetWaylandServer request completes.
  void OnWaylandServerCreated(CapabilityCallback callback,
                              bool success,
                              const base::FilePath& server_path);

  Profile* const profile_;
  // This is owned by Exo, once the server is created.
  BorealisCapabilities* capabilities_ = nullptr;
  base::FilePath server_path_;

  base::WeakPtrFactory<BorealisWaylandInterface> weak_factory_{this};
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_WAYLAND_INTERFACE_H_
