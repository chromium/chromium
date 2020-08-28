// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_FAKE_AMBIENT_BACKEND_CONTROLLER_IMPL_H_
#define ASH_PUBLIC_CPP_AMBIENT_FAKE_AMBIENT_BACKEND_CONTROLLER_IMPL_H_

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// A fake implementation of AmbientBackendController.
class ASH_PUBLIC_EXPORT FakeAmbientBackendControllerImpl
    : public AmbientBackendController {
 public:
  FakeAmbientBackendControllerImpl();
  ~FakeAmbientBackendControllerImpl() override;

  // AmbientBackendController:
  void FetchScreenUpdateInfo(
      int num_topics,
      OnScreenUpdateInfoFetchedCallback callback) override;
  void InitSettings(UpdateSettingsCallback callback) override;
  void GetSettings(GetSettingsCallback callback) override;
  void UpdateSettings(const AmbientSettings& settings,
                      UpdateSettingsCallback callback) override;
  void FetchSettingPreview(int preview_width,
                           int preview_height,
                           OnSettingPreviewFetchedCallback) override;
  void FetchPersonalAlbums(int banner_width,
                           int banner_height,
                           int num_albums,
                           const std::string& resume_token,
                           OnPersonalAlbumsFetchedCallback callback) override;
  void FetchSettingsAndAlbums(
      int banner_width,
      int banner_height,
      int num_albums,
      OnSettingsAndAlbumsFetchedCallback callback) override;
  void SetPhotoRefreshInterval(base::TimeDelta interval) override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_FAKE_AMBIENT_BACKEND_CONTROLLER_IMPL_H_
