// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_REMOVER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_REMOVER_H_

#include <string>
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chromeos/ash/components/dbus/concierge/concierge_service.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace crostini {

class CrostiniRemover : public base::RefCountedThreadSafe<CrostiniRemover> {
 public:
  enum class Result {
    kSuccess,
    kStopVmNoResponse,
    kStopVmFailed,
    kDestroyDiskImageFailed,
  };
  CrostiniRemover(Profile* profile,
                  guest_os::VmType vm_type,
                  std::string vm_name,
                  base::OnceCallback<void(Result)> callback);

  CrostiniRemover(const CrostiniRemover&) = delete;
  CrostiniRemover& operator=(const CrostiniRemover&) = delete;

  void RemoveVm();

 private:
  friend class base::RefCountedThreadSafe<CrostiniRemover>;

  ~CrostiniRemover();

  void StopVmFinished(
      absl::optional<vm_tools::concierge::StopVmResponse> response);
  void DestroyDiskImageFinished(
      absl::optional<vm_tools::concierge::DestroyDiskImageResponse> response);

  Profile* profile_;
  guest_os::VmType vm_type_;
  std::string vm_name_;
  base::OnceCallback<void(Result)> callback_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_REMOVER_H_
