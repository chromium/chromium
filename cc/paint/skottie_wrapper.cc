// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"

#include <utility>
#include <vector>

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"

namespace cc {
namespace {

// Directs logs from the skottie animation builder to //base/logging. Without
// this, errors/warnings from the animation builder get silently dropped.
class SkottieLogWriter : public skottie::Logger {
 public:
  void log(Level level, const char message[], const char* json) override {
    static constexpr char kSkottieLogPrefix[] = "[Skottie] \"";
    static constexpr char kSkottieLogSuffix[] = "\"";
    switch (level) {
      case Level::kWarning:
        LOG(WARNING) << kSkottieLogPrefix << message << kSkottieLogSuffix;
        break;
      case Level::kError:
        LOG(ERROR) << kSkottieLogPrefix << message << kSkottieLogSuffix;
        break;
    }
  }
};

}  // namespace

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::CreateSerializable(
    std::vector<uint8_t> data) {
  base::span<const uint8_t> data_span(data);
  return base::WrapRefCounted<SkottieWrapper>(
      new SkottieWrapper(data_span, std::move(data)));
}

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::CreateNonSerializable(
    base::span<const uint8_t> data) {
  return base::WrapRefCounted<SkottieWrapper>(
      new SkottieWrapper(data, /*owned_data=*/std::vector<uint8_t>()));
}

SkottieWrapper::SkottieWrapper(base::span<const uint8_t> data,
                               std::vector<uint8_t> owned_data)
    : mru_resource_provider_(sk_make_sp<SkottieMRUResourceProvider>()),
      animation_(
          skottie::Animation::Builder()
              .setLogger(sk_make_sp<SkottieLogWriter>())
              .setResourceProvider(skresources::CachingResourceProvider::Make(
                  mru_resource_provider_))
              .make(reinterpret_cast<const char*>(data.data()), data.size())),
      raw_data_(std::move(owned_data)),
      id_(base::FastHash(data)),
      // The underlying assumption here is that skottie::Animation::Builder
      // loads image assets on initialization rather than doing so lazily at
      // render() time. This is the case currently, and there will be unit test
      // failures if this does not hold at some point in the future.
      image_asset_metadata_(mru_resource_provider_->GetImageAssetMetadata()),
      image_assets_(mru_resource_provider_->GetImageAssetMap()) {}

SkottieWrapper::~SkottieWrapper() = default;

const SkottieResourceMetadataMap& SkottieWrapper::GetImageAssetMetadata()
    const {
  return image_asset_metadata_;
}

bool SkottieWrapper::SetImageForAsset(SkottieResourceIdHash asset_id_hash,
                                      sk_sp<SkImage> image,
                                      SkSamplingOptions sampling) {
  auto asset_iter = image_assets_.find(asset_id_hash);
  if (asset_iter == image_assets_.end()) {
    LOG(ERROR) << "Failed to set image for unknown asset with id: "
               << asset_id_hash;
    return false;
  }
  SkottieMRUResourceProvider::FrameData frame_data;
  frame_data.image = std::move(image);
  frame_data.sampling = sampling;
  SkottieMRUResourceProvider::ImageAsset& asset = *asset_iter->second;
  asset.SetCurrentFrameData(std::move(frame_data));
  return true;
}

void SkottieWrapper::Draw(SkCanvas* canvas, float t, const SkRect& rect) {
  base::AutoLock lock(lock_);
  animation_->seek(t);
  animation_->render(canvas, &rect);
}

base::span<const uint8_t> SkottieWrapper::raw_data() const {
  DCHECK(raw_data_.size());
  return base::as_bytes(base::make_span(raw_data_.data(), raw_data_.size()));
}

}  // namespace cc
