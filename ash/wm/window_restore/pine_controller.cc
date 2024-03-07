// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_controller.h"

#include "ash/birch/birch_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/screen_ash.h"
#include "ash/public/cpp/image_util.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_restore/pine_contents_data.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ui/base/display_util.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

// Records the UMA metrics for the pine screenshot taken on the last shutdown.
// Resets the prefs used to store the metrics across shutdowns.
void RecordPineScreenshotMetrics(PrefService* local_state) {
  auto record_uma = [](PrefService* local_state, const std::string& name,
                       const std::string& pref_name) -> void {
    const base::TimeDelta duration = local_state->GetTimeDelta(pref_name);
    // Don't record the metric if we don't have a value.
    if (!duration.is_zero()) {
      base::UmaHistogramTimes(name, duration);
      // Reset the pref in case the next shutdown doesn't take the screenshot.
      local_state->SetTimeDelta(pref_name, base::TimeDelta());
    }
  };

  record_uma(local_state, "Ash.Pine.ScreenshotTakenDuration",
             prefs::kPineScreenshotTakenDuration);
  record_uma(local_state, "Ash.Pine.ScreenshotEncodeAndSaveDuration",
             prefs::kPineScreenshotEncodeAndSaveDuration);
}

bool ShouldShowPineImage(const gfx::ImageSkia& pine_image) {
  if (pine_image.isNull()) {
    return false;
  }

  const gfx::Size image_size = pine_image.size();
  const bool is_image_landscape = image_size.width() > image_size.height();

  // TODO(minch|sammiequon): The pine dialog will only be shown inside the
  // primary display for now. Change the logic here if it changes.
  const display::Display display_with_pine =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  const bool is_display_landscape = chromeos::IsLandscapeOrientation(
      chromeos::GetDisplayCurrentOrientation(display_with_pine));

  // Show the image only if the pine image and the display showing it both have
  // the same orientation.
  return is_image_landscape == is_display_landscape;
}

}  // namespace

PineController::PineController() = default;

PineController::~PineController() = default;

void PineController::MaybeStartPineOverviewSessionDevAccelerator() {
  auto data = std::make_unique<PineContentsData>();
  data->last_session_crashed = false;

  // Chrome.
  data->apps_infos.emplace_back(
      "mgndgikekgjfcpckkfioiadnlibdjbkf", /*tab_title=*/u"Cnn",
      std::vector<GURL>{GURL("https://www.cnn.com/"),
                        GURL("https://www.youtube.com/"),
                        GURL("https://www.google.com/")});
  // Camera.
  data->apps_infos.emplace_back("njfbnohfdkmbmnjapinfcopialeghnmh");
  // Settings.
  data->apps_infos.emplace_back("odknhmnlageboeamepcngndbggdpaobj");
  // Files.
  data->apps_infos.emplace_back("fkiggjmkendpmbegkagpmagjepfkpmeb");
  // Calculator.
  data->apps_infos.emplace_back("oabkinaljpjeilageghcdlnekhphhphl");
  // Chrome.
  data->apps_infos.emplace_back(
      "mgndgikekgjfcpckkfioiadnlibdjbkf", /*tab_title=*/u"Maps",
      std::vector<GURL>{GURL("https://www.google.com/maps/")});
  // Files.
  data->apps_infos.emplace_back("fkiggjmkendpmbegkagpmagjepfkpmeb");
  // Chrome.
  data->apps_infos.emplace_back(
      "mgndgikekgjfcpckkfioiadnlibdjbkf", /*tab_title=*/u"Twitter",
      std::vector<GURL>{GURL("https://www.twitter.com/"),
                        GURL("https://www.youtube.com/"),
                        GURL("https://www.google.com/")});

  MaybeStartPineOverviewSession(std::move(data));
}

void PineController::MaybeStartPineOverviewSession(
    std::unique_ptr<PineContentsData> pine_contents_data) {
  CHECK(features::IsForestFeatureEnabled());

  if (OverviewController::Get()->InOverviewSession()) {
    return;
  }

  // TODO(hewer|sammiequon): This function should only be called once in
  // production code when `pine_contents_data_` is null. It can be called
  // multiple times currently via dev accelerator. Remove this block when
  // `MaybeStartPineOverviewSessionDevAccelerator()` is removed.
  if (pine_contents_data_) {
    StartPineOverviewSession();
    return;
  }

  pine_contents_data_ = std::move(pine_contents_data);

  RecordPineScreenshotMetrics(Shell::Get()->local_state());
  image_util::DecodeImageFile(
      base::BindOnce(&PineController::OnPineImageDecoded,
                     weak_ptr_factory_.GetWeakPtr()),
      GetShutdownPineImagePath(), data_decoder::mojom::ImageCodec::kPng);
}

void PineController::MaybeEndPineOverviewSession() {
  pine_contents_data_.reset();
  OverviewController::Get()->EndOverview(OverviewEndAction::kAccelerator,
                                         OverviewEnterExitType::kPine);
}

void PineController::OnPineImageDecoded(const gfx::ImageSkia& pine_image) {
  CHECK(pine_contents_data_);

  if (ShouldShowPineImage(pine_image)) {
    pine_contents_data_->image = pine_image;
  }

  StartPineOverviewSession();
}

void PineController::StartPineOverviewSession() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceBirchFetch)) {
    LOG(WARNING) << "Forcing Birch data fetch";
    Shell::Get()->birch_model()->RequestBirchDataFetch(base::BindOnce([]() {
      // Dump the items that were fetched.
      LOG(WARNING) << "All items:";
      auto all_items = Shell::Get()->birch_model()->GetAllItems();
      for (const auto& item : all_items) {
        LOG(WARNING) << item->ToString();
      }
      // Dump the items for display.
      LOG(WARNING) << "Items for display:";
      auto display_items = Shell::Get()->birch_model()->GetItemsForDisplay();
      for (const auto& item : display_items) {
        LOG(WARNING) << item->ToString();
      }
    }));
  }
  // TODO(sammiequon): Add a new start action for this type of overview session.
  OverviewController::Get()->StartOverview(OverviewStartAction::kAccelerator,
                                           OverviewEnterExitType::kPine);
}

}  // namespace ash
