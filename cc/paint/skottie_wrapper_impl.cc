// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/skottie_mru_resource_provider.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/modules/skottie/include/Skottie.h"
#include "third_party/skia/modules/skottie/include/SkottieProperty.h"
#include "third_party/skia/modules/skresources/include/SkResources.h"
#include "ui/gfx/geometry/skia_conversions.h"

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

class PropertyHandler final : public skottie::PropertyObserver {
 public:
  PropertyHandler() = default;
  PropertyHandler(const PropertyHandler&) = delete;
  PropertyHandler& operator=(const PropertyHandler&) = delete;
  ~PropertyHandler() override = default;

  void ApplyColorMap(const SkottieColorMap& color_map) {
    for (const auto& map_color : color_map) {
      for (auto& handle : color_handles_[map_color.first]) {
        DCHECK(handle);
        handle->set(map_color.second);
      }
    }
  }

  void ApplyTextMap(const SkottieTextPropertyValueMap& text_map) {
    for (const auto& [node_name_hash, new_text_val] : text_map) {
      auto current_text_values_iter = current_text_values_.find(node_name_hash);
      if (current_text_values_iter == current_text_values_.end()) {
        LOG(WARNING) << "Encountered unknown text node with hash: "
                     << node_name_hash;
        continue;
      }
      current_text_values_iter->second = new_text_val;

      for (auto& handle : text_handles_[node_name_hash]) {
        DCHECK(handle);
        skottie::TextPropertyValue current_text_val = handle->get();
        ConvertTextValueToSkottie(new_text_val, current_text_val);
        handle->set(std::move(current_text_val));
      }
    }
  }

  const SkottieTextPropertyValueMap& current_text_values() const {
    return current_text_values_;
  }

  const SkottieTransformPropertyValueMap current_transform_values() const {
    return current_transform_values_;
  }

  const base::flat_set<std::string>& text_node_names() const {
    return text_node_names_;
  }

  // skottie::PropertyObserver:
  void onColorProperty(
      const char node_name[],
      const LazyHandle<skottie::ColorPropertyHandle>& lh) override {
    if (node_name)
      color_handles_[HashSkottieResourceId(node_name)].push_back(lh());
  }

  void onTextProperty(
      const char node_name[],
      const LazyHandle<skottie::TextPropertyHandle>& lh) override {
    if (!node_name)
      return;

    text_node_names_.insert(node_name);
    SkottieResourceIdHash node_name_hash = HashSkottieResourceId(node_name);
    auto text_handle = lh();
    current_text_values_.emplace(
        node_name_hash, ConvertTextValueToChromium(text_handle->get()));
    text_handles_[node_name_hash].push_back(std::move(text_handle));
  }

  void onTransformProperty(
      const char node_name[],
      const LazyHandle<skottie::TransformPropertyHandle>& lh) override {
    if (!node_name)
      return;

    current_transform_values_.emplace(
        HashSkottieResourceId(node_name),
        ConvertTransformValueToChromium(lh()->get()));
  }

 private:
  static SkottieTextPropertyValue ConvertTextValueToChromium(
      const skottie::TextPropertyValue& value_in) {
    std::string text(value_in.fText.c_str());
    return SkottieTextPropertyValue(std::move(text),
                                    gfx::SkRectToRectF(value_in.fBox));
  }

  static void ConvertTextValueToSkottie(
      const SkottieTextPropertyValue& value_in,
      skottie::TextPropertyValue& value_out) {
    value_out.fText.set(value_in.text().c_str());
    value_out.fBox = gfx::RectFToSkRect(value_in.box());
  }

  static SkottieTransformPropertyValue ConvertTransformValueToChromium(
      const skottie::TransformPropertyValue& value_in) {
    SkottieTransformPropertyValue output = {
        /*position*/ gfx::SkPointToPointF(value_in.fPosition)};
    return output;
  }

  base::flat_map<SkottieResourceIdHash,
                 std::vector<std::unique_ptr<skottie::ColorPropertyHandle>>>
      color_handles_;
  base::flat_map<SkottieResourceIdHash,
                 std::vector<std::unique_ptr<skottie::TextPropertyHandle>>>
      text_handles_;
  base::flat_set<std::string> text_node_names_;
  SkottieTextPropertyValueMap current_text_values_;
  SkottieTransformPropertyValueMap current_transform_values_;
};

class MarkerStore : public skottie::MarkerObserver {
 public:
  MarkerStore() = default;
  MarkerStore(const MarkerStore&) = delete;
  MarkerStore& operator=(const MarkerStore&) = delete;
  ~MarkerStore() override = default;

  // skottie::MarkerObserver implementation:
  void onMarker(const char name[], float t0, float t1) override {
    if (!name)
      return;

    all_markers_.push_back({std::string(name), t0, t1});
  }

  const std::vector<SkottieMarker>& all_markers() const { return all_markers_; }

 private:
  std::vector<SkottieMarker> all_markers_;
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
            sk_make_sp<SkottieMRUResourceProvider>(
                base::BindRepeating(
                    &SkottieWrapperImpl::RunCurrentFrameDataCallback,
                    base::Unretained(this)),
                base::StringPiece(reinterpret_cast<const char*>(data.data()),
                                  data.size()))) {}

  SkottieWrapperImpl(const SkottieWrapperImpl&) = delete;
  SkottieWrapperImpl& operator=(const SkottieWrapperImpl&) = delete;

  // SkottieWrapper implementation:
  bool is_valid() const override { return !!animation_; }

  const SkottieResourceMetadataMap& GetImageAssetMetadata() const override {
    return image_asset_metadata_;
  }

  const base::flat_set<std::string>& GetTextNodeNames() const override
      LOCKS_EXCLUDED(lock_) {
    base::AutoLock lock(lock_);
    return property_handler_->text_node_names();
  }

  SkottieTextPropertyValueMap GetCurrentTextPropertyValues() const override
      LOCKS_EXCLUDED(lock_) {
    base::AutoLock lock(lock_);
    return property_handler_->current_text_values();
  }

  SkottieTransformPropertyValueMap GetCurrentTransformPropertyValues()
      const override LOCKS_EXCLUDED(lock_) {
    base::AutoLock lock(lock_);
    return property_handler_->current_transform_values();
  }

  const std::vector<SkottieMarker>& GetAllMarkers() const override {
    return marker_store_->all_markers();
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
            FrameDataCallback frame_data_cb,
            const SkottieColorMap& color_map,
            const SkottieTextPropertyValueMap& text_map) override
      LOCKS_EXCLUDED(lock_) {
    base::AutoLock lock(lock_);
    current_frame_data_cb_ = std::move(frame_data_cb);
    property_handler_->ApplyColorMap(color_map);
    property_handler_->ApplyTextMap(text_map);
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
      : property_handler_(sk_make_sp<PropertyHandler>()),
        marker_store_(sk_make_sp<MarkerStore>()),
        animation_(
            skottie::Animation::Builder()
                .setLogger(sk_make_sp<SkottieLogWriter>())
                .setPropertyObserver(property_handler_)
                .setResourceProvider(skresources::CachingResourceProvider::Make(
                    mru_resource_provider))
                .setMarkerObserver(marker_store_)
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

  mutable base::Lock lock_;
  FrameDataCallback current_frame_data_cb_ GUARDED_BY(lock_);
  sk_sp<PropertyHandler> property_handler_ GUARDED_BY(lock_);
  const sk_sp<MarkerStore> marker_store_;
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
