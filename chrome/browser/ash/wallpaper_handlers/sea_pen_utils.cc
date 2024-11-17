// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/sea_pen_utils.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_utils_generated.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/manta/proto/manta.pb.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

namespace wallpaper_handlers {

gfx::Size GetLargestDisplaySizeLandscape() {
  // Screen should be non-null if the user is selecting SeaPen thumbnails.
  CHECK(display::Screen::HasScreen());

  gfx::Size largest_size;
  uint64_t largest_area = 0u;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    DVLOG(2) << display.ToString();
    auto next_area = display.GetSizeInPixel().Area64();
    if (next_area > largest_area) {
      largest_size = display.GetSizeInPixel();
      largest_area = next_area;
    }
  }
  DCHECK_GT(largest_area, 0u);

  if (largest_size.height() > largest_size.width()) {
    // Always landscape orientation.
    largest_size.Transpose();
  }

  DVLOG(2) << "largest_size=" << largest_size.ToString();
  return largest_size;
}

bool IsValidOutput(const manta::proto::OutputData& output,
                   std::string_view source) {
  if (!output.has_generation_seed()) {
    LOG(WARNING) << "Manta output data missing id for " << source;
    return false;
  }
  if (!output.has_image() || !output.image().has_serialized_bytes() ||
      output.image().serialized_bytes().empty()) {
    LOG(WARNING) << "Manta output data missing image for" << source;
    return false;
  }
  return true;
}

manta::proto::Request CreateMantaRequest(
    const ash::personalization_app::mojom::SeaPenQueryPtr& query,
    std::optional<uint32_t> generation_seed,
    int num_outputs,
    const gfx::Size& size,
    manta::proto::FeatureName feature_name) {
  DVLOG(2) << __func__ << " generation_seed=" << generation_seed.value_or(0)
           << " num_outputs=" << num_outputs
           << " image_dimensions=" << size.ToString();

  manta::proto::Request request;
  request.set_feature_name(feature_name);

  {
    manta::proto::RequestConfig& request_config =
        *request.mutable_request_config();
    if (generation_seed) {
      request_config.set_generation_seed(*generation_seed);
    }

    // Ignore image_dimensions for CHROMEOS_VC_BACKGROUNDS, since
    // CHROMEOS_VC_BACKGROUNDS returns with default size.
    if (feature_name != manta::proto::FeatureName::CHROMEOS_VC_BACKGROUNDS) {
      manta::proto::ImageDimensions& image_dimensions =
          *request_config.mutable_image_dimensions();
      image_dimensions.set_width(size.width());
      image_dimensions.set_height(size.height());
    }

    request_config.set_num_outputs(num_outputs);
  }

  manta::proto::InputData& input_data = *request.add_input_data();
  if (query->is_text_query()) {
    input_data.set_text(query->get_text_query());
  } else if (query->is_template_query() &&
             ash::IsValidTemplateQuery(query->get_template_query())) {
    input_data.set_tag(kTemplateIdTag.data());
    input_data.set_text(
        ash::TemplateIdToString(query->get_template_query()->id));
    for (auto option : query->get_template_query()->options) {
      manta::proto::InputData& input_option = *request.add_input_data();
      input_option.set_tag(ash::TemplateChipToString(option.first));
      input_option.set_text(ash::TemplateOptionToString(option.second));
    }
  }
  if (ash::features::IsSeaPenUseExptTemplateEnabled()) {
    manta::proto::InputData& expt_template_option = *request.add_input_data();
    expt_template_option.set_tag("use_expt_template");
    expt_template_option.set_text("true");
  }
  return request;
}

std::string GetFeedbackText(
    const ash::personalization_app::mojom::SeaPenQueryPtr& query,
    const ash::personalization_app::mojom::SeaPenFeedbackMetadataPtr&
        metadata) {
  if (query->is_template_query() &&
      !ash::IsValidTemplateQuery(query->get_template_query())) {
    return "";
  }

  std::string feedback_text;
  const char* hashtag = metadata->log_id.starts_with("VcBackground")
                            ? "#VCBackground"
                            : "#AIWallpaper";
  const char* sentiment = metadata->is_positive ? "Positive" : "Negative";
  const char* query_text =
      query->is_text_query()
          ? query->get_text_query().c_str()
          : query->get_template_query()->user_visible_query->text.c_str();
  base::StringAppendF(&feedback_text, "%s %s: %s\n", hashtag, sentiment,
                      query_text);
  if (query->is_template_query()) {
    base::StringAppendF(&feedback_text, "template: %s\n",
                        metadata->log_id.c_str());
    base::StringAppendF(&feedback_text, "options: ");
    for (const auto& [chip, option] : query->get_template_query()->options) {
      base::StringAppendF(&feedback_text, "(%s, %s)",
                          ash::TemplateChipToString(chip).c_str(),
                          ash::TemplateOptionToString(option).c_str());
    }
    base::StringAppendF(&feedback_text, "\n");
  }
  base::StringAppendF(&feedback_text, "generation_seed: %u\n",
                      metadata->generation_seed);
  return feedback_text;
}

}  // namespace wallpaper_handlers
