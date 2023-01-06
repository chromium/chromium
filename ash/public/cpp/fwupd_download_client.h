// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FWUPD_DOWNLOAD_CLIENT_H_
#define ASH_PUBLIC_CPP_FWUPD_DOWNLOAD_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

// Interface for a class that provides profile-based download capabilities to
// ash clients.
// Fwupd is a firmware updater tool that lets users to update their peripheral's
// firmware version.
class ASH_PUBLIC_EXPORT FwupdDownloadClient {
 public:
  FwupdDownloadClient();
  FwupdDownloadClient(const FwupdDownloadClient&) = delete;
  FwupdDownloadClient& operator=(const FwupdDownloadClient&) = delete;

  static FwupdDownloadClient* Get();

  // Return the URL loader factory associated with the active user's profile.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

 protected:
  virtual ~FwupdDownloadClient();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FWUPD_DOWNLOAD_CLIENT_H_
