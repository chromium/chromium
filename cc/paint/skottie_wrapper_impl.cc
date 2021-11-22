// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"

#include <functional>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
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
      : SkottieWrapperImpl(
            data,
            owned_data,
            // * Unretained is safe because SkottieMRUResourceProvider cannot
            //   outlive SkottieWrapperImpl.
            // * Binding "this" in the constructor is safe because the frame
            //   data callback is only triggered during calls to
            //   |animation_->seek()|.
            sk_make_sp<SkottieMRUResourceProvider>(base::BindRepeating(
                &SkottieWrapperImpl::RunCurrentFrameDataCallback,
                base::Unretained(this)))) {}

  SkottieWrapperImpl(const SkottieWrapperImpl&) = delete;
  SkottieWrapperImpl& operator=(const SkottieWrapperImpl&) = delete;

  // SkottieWrapper implementation:
  bool is_valid() const override { return !!animation_; }

  const SkottieResourceMetadataMap& GetImageAssetMetadata() const override {
    return image_asset_metadata_;
  }

  void Seek(float t, FrameDataCallback frame_data_cb) override
      LOCKS_EXCLUDED(lock_) {
    base::AutoLock lock(lock_);
    // There's no need to reset |current_frame_data_cb_| to null when finished.
    // The callback is guaranteed to only be invoked synchronously during calls
    // to |animation_->seek/render()|, and not thereafter.
    current_frame_data_cb_ = std::move(frame_data_cb);
    animation_->seek(t);
  }

  void Draw(SkCanvas* canvas,
            float t,
            const SkRect& rect,
            FrameDataCallback frame_data_cb) override LOCKS_EXCLUDED(lock_) {
    base::AutoLock lock(lock_);
    current_frame_data_cb_ = std::move(frame_data_cb);
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
  SkottieWrapperImpl(
      base::span<const uint8_t> data,
      std::vector<uint8_t> raw_data,
      const sk_sp<SkottieMRUResourceProvider>& mru_resource_provider)
      : animation_(
            skottie::Animation::Builder()
                .setLogger(sk_make_sp<SkottieLogWriter>())
                .setResourceProvider(skresources::CachingResourceProvider::Make(
                    mru_resource_provider))
                .make(reinterpret_cast<const char*>(data.data()), data.size())),
        raw_data_(std::move(raw_data)),
        id_(base::FastHash(data)),
        // The underlying assumption here is that |skottie::Animation::Builder|
        // loads image assets on initialization rather than doing so lazily at
        // |render()| time. This is the case currently, and there will be unit
        // test failures if this does not hold at some point in the future.
        image_asset_metadata_(mru_resource_provider->GetImageAssetMetadata()) {}

  ~SkottieWrapperImpl() override = default;

  FrameDataFetchResult RunCurrentFrameDataCallback(
      SkottieResourceIdHash asset_id_hash,
      float t,
      sk_sp<SkImage>& image_out,
      SkSamplingOptions& sampling_out) EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    lock_.AssertAcquired();
    DCHECK(current_frame_data_cb_);
    return current_frame_data_cb_.Run(asset_id_hash, t, image_out,
                                      sampling_out);
  }

  base::Lock lock_;
  FrameDataCallback current_frame_data_cb_ GUARDED_BY(lock_);
  sk_sp<skottie::Animation> animation_;

  // The raw byte data is stored for serialization across OOP-R. This is only
  // valid if serialization was enabled at construction.
  const std::vector<uint8_t> raw_data_;

  // Unique id generated for a given animation. This will be unique per
  // animation file. 2 animation objects from the same source file will have the
  // same value.
  const uint32_t id_;

  const SkottieResourceMetadataMap image_asset_metadata_;
};

}  // namespace

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::CreateSerializable(
    std::vector<uint8_t> data) {
  base::span<const uint8_t> data_span(data);
  return base::WrapRefCounted(
      new SkottieWrapperImpl(data_span, std::move(data)));
}

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::CreateNonSerializable(
    base::span<const uint8_t> data) {
  return base::WrapRefCounted(
      new SkottieWrapperImpl(data,
                             /*owned_data=*/std::vector<uint8_t>()));
}

}  // namespace cc
