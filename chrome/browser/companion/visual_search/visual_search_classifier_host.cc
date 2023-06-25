// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/visual_search/visual_search_classifier_host.h"

#include "base/base64.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/companion/visual_search/features.h"
#include "chrome/browser/companion/visual_search/visual_search_suggestions_service.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
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

void VisualSearchClassifierHost::HandleClassification(
    std::vector<mojom::VisualSearchSuggestionPtr> results) {
  LOCAL_HISTOGRAM_COUNTS_100("Companion.VisualSearch.ClassificationResultsSize",
                             results.size());
  std::vector<std::string> data_uris;
  data_uris.reserve(results.size());

  // converts list of SkBitmaps to data uris used as img.src for browser.
  for (const auto& result : results) {
    auto data_uri = Base64EncodeBitmap(result->image);
    if (data_uri) {
      data_uris.emplace_back(data_uri.value());
    }
  }

  if (!result_callback_.is_null()) {
    LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.EndClassificationSuccess",
                            !result_callback_.is_null());
    std::move(result_callback_).Run(std::move(data_uris));
  }
  result_callback_.Reset();
  result_handler_.reset();
}

void VisualSearchClassifierHost::StartClassification(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    ResultCallback callback) {
  if (!render_frame_host) {
    LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.EmptyRenderFrame", true);
    return;
  }

  current_url_ = validated_url;
  visual_search_service_->SetModelUpdateCallback(
      base::BindOnce(&VisualSearchClassifierHost::StartClassificationWithModel,
                     weak_ptr_factory_.GetWeakPtr(), render_frame_host,
                     validated_url, std::move(callback)));
}

void VisualSearchClassifierHost::StartClassificationWithModel(
    content::RenderFrameHost* render_frame_host,
    const GURL validated_url,
    ResultCallback callback,
    base::File model,
    std::string base64_config) {
  LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.ModelFileSuccess",
                          model.IsValid());
  if (!model.IsValid()) {
    return;
  }

  if (validated_url != current_url_) {
    LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.MismatchURL", true);
    return;
  }

  absl::optional<std::string> config_switch =
      switches::GetVisualSearchConfigForCompanionOverride();

  // Replace empty string with config switch if we have one.
  if (config_switch) {
    base64_config = std::move(config_switch.value());
  }

  // We set the callback so that we know where to send back the results.
  result_callback_ = std::move(callback);

  mojo::AssociatedRemote<mojom::VisualSuggestionsRequestHandler> visual_search;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &visual_search);
  if (visual_search.is_bound() && !result_handler_.is_bound()) {
    visual_search->StartVisualClassification(
        std::move(model), base64_config,
        result_handler_.BindNewPipeAndPassRemote());
  } else {
    // Closing file in background thread since we did not make IPC.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&CloseModelFile, std::move(model)));
  }

  LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.StartClassificationSuccess",
                          visual_search.is_bound());
}

void VisualSearchClassifierHost::CancelClassification() {
  result_callback_.Reset();
  current_url_ = GURL::EmptyGURL();
  LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualSearch.ClassificationCancelled",
                          true);
}
}  // namespace companion::visual_search
