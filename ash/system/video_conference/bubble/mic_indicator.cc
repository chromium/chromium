// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/system/video_conference/bubble/mic_indicator.h"

#include <cmath>
#include <iomanip>
#include <iostream>

#include "ash/style/ash_color_id.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"

namespace ash::video_conference {

namespace {

const int kIndicatorLines = 4;
const int kIndicatorSpace = 2;
const int kIndicatorWidth = 2;
const int kIndicatorTotalWidth =
    kIndicatorLines * kIndicatorWidth + (kIndicatorLines - 1) * kIndicatorSpace;
const float kIndicatorLengths[] = {0.4, 0.9, 0.6, 0.85};

// Powers above kLogEwmaMax will be restricted to this value.
const float kLogEwmaMax = std::log(0.02);
// Powers below kLogEwmaMin will be restricted to this value.
const float kLogEwmaMin = std::log(0.00002);
const float kLogEwmaDiff = kLogEwmaMax - kLogEwmaMin;

constexpr int kMaxStep = 8;
constexpr float phaseLengths[] = {1.0, 1.1, 1.3, 1.1, 1.0, 0.9, 0.7, 0.9};
constexpr auto kMicIndicatorInsets = gfx::Insets::TLBR(10, 10, 10, 10);

float ScalePower(float power) {
  // Adjust the power on a logarithmic scale, allowing for more noticeable
  // changes at lower volumes.
  float log_value = std::log(power);
  if (log_value > kLogEwmaMax) {
    log_value = kLogEwmaMax;
  }
  if (log_value < kLogEwmaMin) {
    log_value = kLogEwmaMin;
  }
  float normalized = (log_value - kLogEwmaMin) / (kLogEwmaDiff);
  return 0.1 + normalized * (1.0 - 0.1);
}

}  // namespace

MicIndicator::MicIndicator() {
  VideoConferenceTrayController::Get()->SetEwmaPowerReportEnabled(true);
  SetInsideBorderInsets(kMicIndicatorInsets);

  power_ = VideoConferenceTrayController::Get()->GetEwmaPower();
  step_ = 0;
  color_ = cros_tokens::kCrosSysOnSurface;

  timer_ = std::make_unique<base::RepeatingTimer>();
  timer_->Start(FROM_HERE, base::Milliseconds(30),
                base::BindRepeating(&MicIndicator::UpdateProgress,
                                    base::Unretained(this)));
}

MicIndicator::~MicIndicator() {
  // Disable ewma power reporting when the view is destructed, so CRAS
  // doesn't report unnecessary data.
  VideoConferenceTrayController::Get()->SetEwmaPowerReportEnabled(false);
}

void MicIndicator::UpdateProgress() {
  step_ = (step_ + 1) % kMaxStep;
  if (step_ == 0) {
    bool sidetone_enabled =
        VideoConferenceTrayController::Get()->GetSidetoneEnabled();
    color_ = sidetone_enabled ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                              : cros_tokens::kCrosSysOnSurface;
    power_ = VideoConferenceTrayController::Get()->GetEwmaPower();
  }
  SchedulePaint();
}

void MicIndicator::OnPaint(gfx::Canvas* canvas) {
  const float multiplier = ScalePower(power_);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStrokeWidth(kIndicatorWidth);
  flags.setColor(GetColorProvider()->GetColor(color_));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  SkPath path;

  const int view_height = GetContentsBounds().height();
  const int view_width = GetContentsBounds().width();
  float x = (view_width - kIndicatorTotalWidth) / 2;
  for (int i = 0; i < kIndicatorLines; i++) {
    float length = view_height *
                   // Set each line's animation cycle based on the previous
                   // line, creating a wave-like effect.
                   phaseLengths[(step_ + kMaxStep - i) % kMaxStep] *
                   kIndicatorLengths[i];

    length = length * multiplier;

    float y0 = (view_height - length) / 2;
    float y1 = y0 + length;
    path.moveTo(x, y0);
    path.lineTo(x, y1);
    canvas->DrawPath(path, flags);

    x += kIndicatorSpace + static_cast<int>(flags.getStrokeWidth());
  }
}

BEGIN_METADATA(MicIndicator)
END_METADATA

}  // namespace ash::video_conference
