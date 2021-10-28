// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"

#include <utility>
#include <vector>

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/skottie_mru_resource_provider.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/modules/skottie/include/Skottie.h"
#include "third_party/skia/modules/skresources/include/SkResources.h"

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

class SkottieWrapperImpl : public SkottieWrapper {
 public:
  SkottieWrapperImpl(base::span<const uint8_t> data,
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
        // render() time. This is the case currently, and there will be unit
        // test failures if this does not hold at some point in the future.
        image_asset_metadata_(mru_resource_provider_->GetImageAssetMetadata()),
        image_assets_(mru_resource_provider_->GetImageAssetMap()) {}

  SkottieWrapperImpl(const SkottieWrapperImpl&) = delete;
  SkottieWrapperImpl& operator=(const SkottieWrapperImpl&) = delete;

  // SkottieWrapper implementation:
  bool is_valid() const override { return !!animation_; }

  const SkottieResourceMetadataMap& GetImageAssetMetadata() const override {
    return image_asset_metadata_;
  }

  bool SetImageForAsset(SkottieResourceIdHash asset_id_hash,
                        sk_sp<SkImage> image,
                        SkSamplingOptions sampling) override {
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

  void Draw(SkCanvas* canvas, float t, const SkRect& rect) override {
    base::AutoLock lock(lock_);
    animation_->seek(t);
    animation_->render(canvas, &rect);
  }

  float duration() const override { return animation_->duration(); }

  SkSize size() const override { return animation_->size(); }

  base::span<const uint8_t> raw_data() const override {
    DCHECK(raw_data_.size());
    return base::as_bytes(base::make_span(raw_data_.data(), raw_data_.size()));
  }

  uint32_t id() const override { return id_; }

 private:
  friend class base::RefCountedThreadSafe<SkottieWrapperImpl>;

  ~SkottieWrapperImpl() override = default;

  base::Lock lock_;
  const sk_sp<SkottieMRUResourceProvider> mru_resource_provider_;
  sk_sp<skottie::Animation> animation_;

  // The raw byte data is stored for serialization across OOP-R. This is only
  // valid if serialization was enabled at construction.
  const std::vector<uint8_t> raw_data_;

  // Unique id generated for a given animation. This will be unique per
  // animation file. 2 animation objects from the same source file will have the
  // same value.
  const uint32_t id_;

  const SkottieResourceMetadataMap image_asset_metadata_;
  const SkottieMRUResourceProvider::ImageAssetMap image_assets_;
};

}  // namespace

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::CreateSerializable(
    std::vector<uint8_t> data) {
  base::span<const uint8_t> data_span(data);
  return base::MakeRefCounted<SkottieWrapperImpl>(data_span, std::move(data));
}

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::CreateNonSerializable(
    base::span<const uint8_t> data) {
  return base::MakeRefCounted<SkottieWrapperImpl>(
      data, /*owned_data=*/std::vector<uint8_t>());
}

}  // namespace cc
