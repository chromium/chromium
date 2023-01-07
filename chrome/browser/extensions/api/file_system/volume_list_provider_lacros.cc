// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/volume_list_provider_lacros.h"

#include "base/logging.h"
#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate_lacros.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/lacros/lacros_service.h"

namespace extensions {

VolumeListProviderLacros::VolumeListProviderLacros(Profile* profile)
    : profile_(profile) {}

VolumeListProviderLacros::~VolumeListProviderLacros() = default;

void VolumeListProviderLacros::Start() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::VolumeManager>())
    return;
  lacros_service->GetRemote<crosapi::mojom::VolumeManager>()
      ->AddVolumeListObserver(receiver_.BindNewPipeAndPassRemoteWithVersion());
}

void VolumeListProviderLacros::OnVolumeListChanged(
    std::vector<crosapi::mojom::VolumePtr> volume_list) {
  DCHECK(profile_);
  file_system_api::DispatchVolumeListChangeEventLacros(profile_, volume_list);
}

}  // namespace extensions
