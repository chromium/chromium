// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_video_ui_launcher.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/ui/ambient_video_view.h"
#include "ash/public/cpp/personalization_app/time_of_day_paths.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

base::FilePath GetVideoHtmlPath() {
  return personalization_app::GetTimeOfDaySrcDir().Append(
      personalization_app::kAmbientVideoHtml);
}

base::StringPiece GetVideoFile(AmbientVideo video) {
  switch (video) {
    case AmbientVideo::kNewMexico:
      return personalization_app::kTimeOfDayNewMexicoVideo;
    case AmbientVideo::kClouds:
      return personalization_app::kTimeOfDayCloudsVideo;
  }
}

}  // namespace

AmbientVideoUiLauncher::AmbientVideoUiLauncher(
    PrefService* pref_service,
    AmbientViewDelegate* view_delegate)
    : pref_service_(pref_service), view_delegate_(view_delegate) {
  CHECK(pref_service_);
}
AmbientVideoUiLauncher::~AmbientVideoUiLauncher() = default;

void AmbientVideoUiLauncher::Initialize(InitializationCallback on_done) {
  CHECK(on_done);
  CHECK(!is_active_);
  is_active_ = true;
  AmbientUiSettings ui_settings =
      AmbientUiSettings::ReadFromPrefService(*pref_service_);
  CHECK(ui_settings.video())
      << "AmbientVideoUiLauncher should not be active for "
      << ToString(ui_settings.theme());
  current_video_ = *ui_settings.video();
  weather_refresher_ = Shell::Get()
                           ->ambient_controller()
                           ->ambient_weather_controller()
                           ->CreateScopedRefresher();
  std::move(on_done).Run(/*success=*/true);
}

std::unique_ptr<views::View> AmbientVideoUiLauncher::CreateView() {
  CHECK(is_active_);
  return std::make_unique<AmbientVideoView>(GetVideoFile(current_video_),
                                            GetVideoHtmlPath(), current_video_,
                                            view_delegate_);
}

void AmbientVideoUiLauncher::Finalize() {
  weather_refresher_.reset();
  is_active_ = false;
}

AmbientBackendModel* AmbientVideoUiLauncher::GetAmbientBackendModel() {
  return nullptr;
}

bool AmbientVideoUiLauncher::IsActive() {
  return is_active_;
}

}  // namespace ash
