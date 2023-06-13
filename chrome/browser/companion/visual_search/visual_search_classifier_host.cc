// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/visual_search/visual_search_classifier_host.h"

#include "base/base64.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/companion/visual_search/features.h"
#include "chrome/browser/companion/visual_search/visual_search_suggestions_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"
#include "ui/gfx/image/buffer_w_stream.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "url/gurl.h"

namespace companion::visual_search {

namespace {
absl::optional<std::string> Base64EncodeBitmap(const SkBitmap& bitmap) {
  gfx::BufferWStream stream;
  const bool encoding_succeeded =
      SkJpegEncoder::Encode(&stream, bitmap.pixmap(), {});

  if (!encoding_succeeded) {
    return absl::nullopt;
  }

  base::StringPiece mime_subtype = "jpg";
  std::string result = "data:image/";
  result.append(mime_subtype.begin(), mime_subtype.end());
  result.append(";base64,");
  result.append(
      base::Base64Encode(base::as_bytes(base::make_span(stream.TakeBuffer()))));
  return result;
}

// Close the provided model file.
void CloseModelFile(base::File model_file) {
  if (!model_file.IsValid()) {
    return;
  }
  model_file.Close();
}
}  // namespace

VisualSearchClassifierHost::VisualSearchClassifierHost(
    VisualSearchSuggestionsService* visual_search_service)
    : visual_search_service_(visual_search_service) {}

VisualSearchClassifierHost::~VisualSearchClassifierHost() = default;

void VisualSearchClassifierHost::OnClassificationResult(
    const std::vector<SkBitmap>& images) {
  std::vector<std::string> data_uris;
  data_uris.reserve(images.size());
  LOCAL_HISTOGRAM_COUNTS_100("Companion.VisualSearch.ClassificationResultsSize",
                             images.size());

  // converts list of SkBitmaps to data uris used as img.src for browser.
  for (const auto& image : images) {
    auto data_uri = Base64EncodeBitmap(image);
    if (data_uri) {
      data_uris.push_back(data_uri.value());
    }
  }

  // TODO(b/284648407): Do mojom IPC to side panel using mojom::CompanionPage.
}

// TODO(pstjuste): RenderFrameHost is used to setup IPC communication with
// renderer process via the InterfaceRegistry. Eventually, the url will be
// used for caching visual search suggestions without having to do IPC.
void VisualSearchClassifierHost::StartClassification(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  base::File model = visual_search_service_->GetModelFile();
  LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.ModelFileSuccess",
                          model.IsValid());
  if (!model.IsValid()) {
    return;
  }

  std::string base64_config;
  absl::optional<std::string> config_switch =
      switches::GetVisualSearchConfigForCompanionOverride();

  // Replace empty string with config switch if we have one.
  if (config_switch) {
    base64_config = std::move(config_switch.value());
  }

  VLOG(1) << "ClassificationSuccess " << classifier_agent_.is_null();
  LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.StartClassificationSuccess",
                          !classifier_agent_.is_null());
  if (!classifier_agent_.is_null()) {
    std::move(classifier_agent_).Run(0, std::move(model), base64_config);
  } else {
    // Closing file in background thread since we did not make IPC.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&CloseModelFile, std::move(model)));
  }
}

void VisualSearchClassifierHost::SetClassifierAgentForTesting(
    ClassifierAgent agent) {
  classifier_agent_ = std::move(agent);
}
}  // namespace companion::visual_search
