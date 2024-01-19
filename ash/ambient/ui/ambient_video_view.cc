// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_video_view.h"

#include <string_view>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/ambient/ui/ambient_slideshow_peripheral_ui.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/strcat.h"

#include "base/time/time.h"
#include "net/base/url_util.h"
#include "ui/views/layout/fill_layout.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace ash {

namespace {

constexpr std::string_view kAmbientVideoFileQueryParam = "video_file";

// Apply the same jitter interval to the peripheral elements as the slideshow
// theme does (which applies jitter each time the photo switches).
constexpr base::TimeDelta kPeripheralUiJitterPeriod = kPhotoRefreshInterval;

GURL BuildFileUrl(const base::FilePath& file_path) {
  return GURL(base::StrCat(
      {url::kFileScheme, url::kStandardSchemeSeparator, file_path.value()}));
}

}  // namespace

AmbientVideoView::AmbientVideoView(std::string_view video_file,
                                   const base::FilePath& html_path,
                                   AmbientVideo video,
                                   AmbientViewDelegate* view_delegate)
    : video_(video),
      peripheral_ui_(
          std::make_unique<AmbientSlideshowPeripheralUi>(view_delegate)) {
  DCHECK(!video_file.empty());
  DCHECK(!html_path.empty());
  DCHECK(AshWebViewFactory::Get());
  SetUseDefaultFillLayout(true);
  AshWebView::InitParams web_view_params;
  // Disables wake locks so the video doesn't stop the device from going to
  // sleep.
  web_view_params.enable_wake_locks = false;
  ash_web_view_ =
      AddChildView(AshWebViewFactory::Get()->Create(web_view_params));
  ash_web_view_->SetID(kAmbientVideoWebView);
  ash_web_view_->SetUseDefaultFillLayout(true);
  GURL ambient_video_url = net::AppendQueryParameter(
      BuildFileUrl(html_path), kAmbientVideoFileQueryParam, video_file);
  ash_web_view_->Navigate(ambient_video_url);

  AddChildView(peripheral_ui_.get());
  peripheral_ui_->UpdateLeftPaddingToMatchBottom();
  // Update details label to empty string as details info is not shown for
  // ambient video.
  peripheral_ui_->UpdateImageDetails(u"", u"");
  peripheral_ui_jitter_timer_.Start(
      FROM_HERE, kPeripheralUiJitterPeriod, peripheral_ui_.get(),
      &AmbientSlideshowPeripheralUi::UpdateGlanceableInfoPosition);
}

AmbientVideoView::~AmbientVideoView() {
  AmbientUiSettings ui_settings(
      personalization_app::mojom::AmbientTheme::kVideo, video_);
  ambient::RecordAmbientModeVideoSessionStatus(ash_web_view_.get(),
                                               ui_settings);
  ambient::RecordAmbientModeVideoSmoothness(ash_web_view_.get(), ui_settings);
}

}  // namespace ash
