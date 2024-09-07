// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fwupd/fwupd_download_client_impl.h"

#include "base/check_is_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

FwupdDownloadClientImpl::FwupdDownloadClientImpl() = default;
FwupdDownloadClientImpl::~FwupdDownloadClientImpl() = default;

scoped_refptr<network::SharedURLLoaderFactory>
FwupdDownloadClientImpl::GetURLLoaderFactory() {
  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();

  Profile* profile = active_user
                         ? ProfileHelper::Get()->GetProfileByUser(active_user)
                         : nullptr;

  // Active user and profile might not be initialized in some tests
  if (!profile) {
    CHECK_IS_TEST();
    return nullptr;
  }

  return profile->GetURLLoaderFactory();
}

}  // namespace ash
