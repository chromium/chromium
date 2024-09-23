// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/skottie_mru_resource_provider.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkFontMgr.h"
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

// Methods for converting from a skottie::<Type>PropertyValue to its
// corresponding representation in Chromium that gets circulated through the
// graphics pipeline.
class SkottiePropertyConversions {
 public:
  // Convert skottie library representation to Chromium representation:
  static SkColor ConvertToChromium(
      const skottie::ColorPropertyValue& skottie_value) {
    return skottie_value;
  }

  static SkottieTextPropertyValue ConvertToChromium(
      const skottie::TextPropertyValue& skottie_value) {
    std::string text(skottie_value.fText.c_str());
    return SkottieTextPropertyValue(std::move(text),
                                    gfx::SkRectToRectF(skottie_value.fBox));
  }

  static SkottieTransformPropertyValue ConvertToChromium(
      const skottie::TransformPropertyValue& skottie_value) {
    SkottieTransformPropertyValue output = {
        /*position*/ gfx::SkPointToPointF(skottie_value.fPosition)};
    return output;
  }

  // Convert Chromium representation to skottie library representation:
  static void ConvertToSkottie(const SkColor& chromium_value,
                               skottie::ColorPropertyValue& skottie_value_out) {
    skottie_value_out = chromium_value;
  }

  static void ConvertToSkottie(const SkottieTextPropertyValue& chromium_value,
                               skottie::TextPropertyValue& skottie_value_out) {
    skottie_value_out.fText.set(chromium_value.text().c_str());
    skottie_value_out.fBox = gfx::RectFToSkRect(chromium_value.box());
  }

  static void ConvertToSkottie(
      const SkottieTransformPropertyValue& chromium_value,
      skottie::TransformPropertyValue& skottie_value_out) {
    NOTIMPLEMENTED() << "No use case yet for modifying transform properties";
  }
};

template <typename SkottiePropertyValueType,
          typename SkottiePropertyHandleType,
          typename ChromiumPropertyValueType>
class PropertyManager {
 public:
  PropertyManager() = default;
  PropertyManager(const PropertyManager&) = delete;
  PropertyManager& operator=(const PropertyManager&) = delete;
  ~PropertyManager() = default;

  void OnNodeParsed(
      const char node_name[],
      const skottie::PropertyObserver::LazyHandle<SkottiePropertyHandleType>&
          lh) {
    if (!node_name)
      return;

    node_names_.insert(node_name);
    SkottieResourceIdHash node_name_hash = HashSkottieResourceId(node_name);
    auto skottie_property_handle = lh();
    current_vals_as_chromium_.emplace(
        node_name_hash, SkottiePropertyConversions::ConvertToChromium(
                            skottie_property_handle->get()));
    skottie_property_handles_[node_name_hash].push_back(
        std::move(skottie_property_handle));
  }

  void SetNewValues(
      const base::flat_map<SkottieResourceIdHash, ChromiumPropertyValueType>&
          new_vals_as_chromium) {
    for (const auto& [node_name_hash, new_val_as_chromium] :
         new_vals_as_chromium) {
      auto iter = current_vals_as_chromium_.find(node_name_hash);
      if (iter == current_vals_as_chromium_.end()) {
        LOG(WARNING) << "Encountered unknown property node with hash: "
                     << node_name_hash;
        continue;
      }
      iter->second = new_val_as_chromium;

      for (auto& skottie_handle : skottie_property_handles_[node_name_hash]) {
        DCHECK(skottie_handle);
        SkottiePropertyValueType current_val_as_skottie = skottie_handle->get();
        SkottiePropertyConversions::ConvertToSkottie(new_val_as_chromium,
                                                     current_val_as_skottie);
        skottie_handle->set(std::move(current_val_as_skottie));
      }
    }
  }

  const base::flat_set<std::string>& node_names() const { return node_names_; }

  const base::flat_map<SkottieResourceIdHash, ChromiumPropertyValueType>
  current_vals_as_chromium() const {
    return current_vals_as_chromium_;
  }

 private:
  base::flat_map<SkottieResourceIdHash,
                 std::vector<std::unique_ptr<SkottiePropertyHandleType>>>
      skottie_property_handles_;
  base::flat_set<std::string> node_names_;
  base::flat_map<SkottieResourceIdHash, ChromiumPropertyValueType>
      current_vals_as_chromium_;
};

using ColorPropertyManager = PropertyManager<skottie::ColorPropertyValue,
                                             skottie::ColorPropertyHandle,
                                             SkColor>;
using TextPropertyManager = PropertyManager<skottie::TextPropertyValue,
                                            skottie::TextPropertyHandle,
                                            SkottieTextPropertyValue>;
using TransformPropertyManager =
    PropertyManager<skottie::TransformPropertyValue,
                    skottie::TransformPropertyHandle,
                    SkottieTransformPropertyValue>;

class GlobalPropertyManager final : public skottie::PropertyObserver {
 public:
  GlobalPropertyManager() = default;
  GlobalPropertyManager(const GlobalPropertyManager&) = delete;
  GlobalPropertyManager& operator=(const GlobalPropertyManager&) = delete;
  ~GlobalPropertyManager() override = default;

  ColorPropertyManager& color_property_manager() {
    return color_property_manager_;
  }

  TextPropertyManager& text_property_manager() {
    return text_property_manager_;
  }

  TransformPropertyManager& transform_property_manager() {
    return transform_property_manager_;
  }

  // skottie::PropertyObserver:
  void onColorProperty(
      const char node_name[],
      const LazyHandle<skottie::ColorPropertyHandle>& lh) override {
    color_property_manager_.OnNodeParsed(node_name, lh);
  }

  void onTextProperty(
      const char node_name[],
      const LazyHandle<skottie::TextPropertyHandle>& lh) override {
    text_property_manager_.OnNodeParsed(node_name, lh);
  }

  void onTransformProperty(
      const char node_name[],
      const LazyHandle<skottie::TransformPropertyHandle>& lh) override {
    transform_property_manager_.OnNodeParsed(node_name, lh);
  }

 private:
  ColorPropertyManager color_property_manager_;
  TextPropertyManager text_property_manager_;
  TransformPropertyManager transform_property_manager_;
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
                std::string_view(reinterpret_cast<const char*>(data.data()),
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
    return property_manager_->text_property_manager().node_names();
  }

  SkottieTextPropertyValueMap GetCurrentTextPropertyValues() const override
      LOCKS_EXCLUDED(lock_) {
    base::AutoLock lock(lock_);
    return property_manager_->text_property_manager()
        .current_vals_as_chromium();
  }

  SkottieTransformPropertyValueMap GetCurrentTransformPropertyValues()
      const override LOCKS_EXCLUDED(lock_) {
    base::AutoLock lock(lock_);
    return property_manager_->transform_property_manager()
        .current_vals_as_chromium();
  }

  SkottieColorMap GetCurrentColorPropertyValues() const override
      LOCKS_EXCLUDED(lock_) {
    base::AutoLock lock(lock_);
    return property_manager_->color_property_manager()
        .current_vals_as_chromium();
  }

  const std::vector<SkottieMarker>& GetAllMarkers() const override {
    return marker_store_->all_markers();
  }

  void Seek(float t, FrameDataCallback frame_data_cb) override
      LOCKS_EXCLUDED(lock_) {
    TRACE_EVENT1("cc", "SkottieWrapperImpl::Seek", "timestamp", t);
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
    TRACE_EVENT1("cc", "SkottieWrapperImpl::Draw", "timestamp", t);
    base::AutoLock lock(lock_);
    current_frame_data_cb_ = std::move(frame_data_cb);
    property_manager_->color_property_manager().SetNewValues(color_map);
    property_manager_->text_property_manager().SetNewValues(text_map);
    animation_->seek(t);
    animation_->render(canvas, &rect);
  }

  float duration() const override { return animation_->duration(); }

  SkSize size() const override { return animation_->size(); }

  base::span<const uint8_t> raw_data() const override {
    DCHECK(raw_data_.size());
    return raw_data_;
  }

  uint32_t id() const override { return id_; }

 private:
  SkottieWrapperImpl(
      base::span<const uint8_t> data,
      std::vector<uint8_t> raw_data,
      const sk_sp<SkottieMRUResourceProvider>& mru_resource_provider)
      : property_manager_(sk_make_sp<GlobalPropertyManager>()),
        marker_store_(sk_make_sp<MarkerStore>()),
        animation_(
            skottie::Animation::Builder()
                .setLogger(sk_make_sp<SkottieLogWriter>())
                .setPropertyObserver(property_manager_)
                .setFontManager(skia::DefaultFontMgr())
                .setResourceProvider(skresources::CachingResourceProvider::Make(
                    skresources::DataURIResourceProviderProxy::Make(
                        mru_resource_provider)))
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
  sk_sp<GlobalPropertyManager> property_manager_ GUARDED_BY(lock_);
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
scoped_refptr<SkottieWrapper> SkottieWrapper::UnsafeCreateSerializable(
    std::vector<uint8_t> data) {
  base::span<const uint8_t> data_span(data);
  return base::WrapRefCounted(
      new SkottieWrapperImpl(data_span, std::move(data)));
}

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::UnsafeCreateNonSerializable(
    base::span<const uint8_t> data) {
  return base::WrapRefCounted(
      new SkottieWrapperImpl(data,
                             /*owned_data=*/std::vector<uint8_t>()));
}

}  // namespace cc
