// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_REMOVER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_REMOVER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"

class Profile;

namespace guest_os {

class GuestOsRemover : public base::RefCountedThreadSafe<GuestOsRemover> {
 public:
  enum class Result {
    kSuccess,
    kStopVmNoResponse,
    kStopVmFailed,
    kDestroyDiskImageFailed,
  };
  GuestOsRemover(Profile* profile,
                 guest_os::VmType vm_type,
                 std::string vm_name,
                 base::OnceCallback<void(Result)> callback);

  GuestOsRemover(const GuestOsRemover&) = delete;
  GuestOsRemover& operator=(const GuestOsRemover&) = delete;

  void RemoveVm();

 private:
  friend class base::RefCountedThreadSafe<GuestOsRemover>;

  ~GuestOsRemover();

  void StopVmFinished(
      std::optional<vm_tools::concierge::StopVmResponse> response);
  void DestroyDiskImageFinished(
      std::optional<vm_tools::concierge::DestroyDiskImageResponse> response);

  raw_ptr<Profile> profile_;
  guest_os::VmType vm_type_;
  std::string vm_name_;
  base::OnceCallback<void(Result)> callback_;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_REMOVER_H_
