// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/image_util.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_restore/pine_contents_data.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
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

}  // namespace

PineController::PineController() = default;

PineController::~PineController() = default;

void PineController::MaybeStartPineOverviewSessionDevAccelerator() {
  auto data = std::make_unique<PineContentsData>();
  data->last_session_crashed = false;

  // Chrome.
  data->apps_infos.emplace_back(
      "mgndgikekgjfcpckkfioiadnlibdjbkf", /*tab_title=*/"Cnn",
      std::vector<std::string>{"https://www.cnn.com/",
                               "https://www.youtube.com/",
                               "https://www.google.com/"});
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
      "mgndgikekgjfcpckkfioiadnlibdjbkf", /*tab_title=*/"Maps",
      std::vector<std::string>{"https://www.google.com/maps/"});
  // Files.
  data->apps_infos.emplace_back("fkiggjmkendpmbegkagpmagjepfkpmeb");
  // Chrome.
  data->apps_infos.emplace_back(
      "mgndgikekgjfcpckkfioiadnlibdjbkf", /*tab_title=*/"Twitter",
      std::vector<std::string>{"https://www.twitter.com/",
                               "https://www.youtube.com/",
                               "https://www.google.com/"});

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

  // TODO(minch|sammiequon): Record the metrics on start up when determining
  // whether to show the pine dialog.
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
  pine_contents_data_->image = pine_image;

  StartPineOverviewSession();
}

void PineController::StartPineOverviewSession() {
  // TODO(sammiequon): Add a new start action for this type of overview session.
  OverviewController::Get()->StartOverview(OverviewStartAction::kAccelerator,
                                           OverviewEnterExitType::kPine);
}

}  // namespace ash
