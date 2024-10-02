// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/watermark_view.h"

#include <math.h>

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "cc/paint/paint_canvas.h"
#include "chrome/common/channel_info.h"
#include "components/enterprise/watermarking/watermark.h"
#include "components/version_info/version_info.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/render_text.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace {
constexpr float kTextSize = 24.0f;
constexpr int kWatermarkBlockWidth = 350;

// The following values are calculated from the required percentages, 4% for
// fill opacity and 6% for outline opacity.
constexpr int kDefaultFillOpacity = 0xb;
constexpr int kDefaultOutlineOpacity = 0x11;

// Default color values for outline and fill.
constexpr SkColor kFillColor =
    SkColorSetARGB(kDefaultFillOpacity, 0x00, 0x00, 0x00);
constexpr SkColor kOutlineColor =
    SkColorSetARGB(kDefaultOutlineOpacity, 0xff, 0xff, 0xff);

// Command line switches that allow users to set fill and outline opacity values
// as a percentage between 1 and 99, inclusive.
constexpr char kWatermarkFillOpacityPercentFlag[] = "watermark-fill-opacity";
constexpr char kWatermarkOutlineOpacityPercentFlag[] =
    "watermark-outline-opacity";

int GetClampedOpacity(int default_value, const char* cmd_opacity_percent_flag) {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(cmd_opacity_percent_flag) &&
      (chrome::GetChannel() != version_info::Channel::STABLE ||
       chrome::GetChannel() != version_info::Channel::BETA)) {
    int opacity = default_value;
    if (base::StringToInt(cmd->GetSwitchValueASCII(cmd_opacity_percent_flag),
                          &opacity)) {
      return std::clamp(opacity, 1, 99) * 256 / 100;
    }
  }
  return default_value;
}

SkColor GetFillColor() {
  int opacity =
      GetClampedOpacity(kDefaultFillOpacity, kWatermarkFillOpacityPercentFlag);
  return SkColorSetA(kFillColor, opacity);
}

SkColor GetOutlineColor() {
  int opacity = GetClampedOpacity(kDefaultOutlineOpacity,
                                  kWatermarkOutlineOpacityPercentFlag);
  return SkColorSetA(kOutlineColor, opacity);
}
}  // namespace

namespace enterprise_watermark {

WatermarkView::WatermarkView() : WatermarkView(std::string("")) {}

WatermarkView::WatermarkView(std::string text)
    : background_color_(SkColorSetARGB(0, 0, 0, 0)) {
  SetCanProcessEventsWithinSubtree(false);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetString(text);
}

WatermarkView::~WatermarkView() = default;

void WatermarkView::SetString(const std::string& text) {
  DCHECK(base::IsStringUTF8(text));

  if (text.empty()) {
    text_fill_.reset();
    text_outline_.reset();
    block_height_ = 0;
  } else {
    std::u16string utf16_text = base::UTF8ToUTF16(text);

    // The coordinates here do not matter as the display rect will change for
    // each drawn block.
    gfx::Rect display_rect(0, 0, kWatermarkBlockWidth, 0);
    text_fill_ = CreateFillRenderText(display_rect, utf16_text, GetFillColor());
    text_outline_ =
        CreateOutlineRenderText(display_rect, utf16_text, GetOutlineColor());

    // `block_height_` is going to be the max required height for a single line
    // times the number of line.
    block_height_ = GetWatermarkBlockHeight(
        utf16_text, text_fill_->GetNumLines(), kWatermarkBlockWidth, kTextSize);
  }

  // Invalidate the state of the view.
  SchedulePaint();
}

void WatermarkView::OnPaint(gfx::Canvas* canvas) {
  // Trying to render an empty string in Skia will fail. A string is required
  // to create the command buffer for the renderer.
  DrawWatermark(canvas, text_fill_.get(), text_outline_.get(), block_height_,
                GetContentsBounds(), kWatermarkBlockWidth);
}

void WatermarkView::SetBackgroundColor(SkColor background_color) {
  background_color_ = background_color;
  SchedulePaint();
}

void WatermarkView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->AddState(ax::mojom::State::kInvisible);
}

BEGIN_METADATA(WatermarkView)
END_METADATA

}  // namespace enterprise_watermark
