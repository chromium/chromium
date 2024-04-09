// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_WRAPPER_H_
#define CC_PAINT_SKOTTIE_WRAPPER_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_marker.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_text_property_value.h"
#include "cc/paint/skottie_transform_property_value.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"

class SkCanvas;
class SkImage;
struct SkRect;
struct SkSize;

namespace cc {

// A wrapper over Skia's Skottie object that can be shared by multiple
// SkiaVectorAnimation objects. This class is thread safe when performing a draw
// on an SkCanvas.
//
// The API intentionally does not have any dependencies on the Skottie public
// header files. This is to facilitate a "dummy" implementation for builds where
// the Skottie library should not be compiled/linked into the final binary.
class CC_PAINT_EXPORT SkottieWrapper
    : public base::RefCountedThreadSafe<SkottieWrapper> {
 public:
  // Factory methods are "Unsafe" because they assume the `data` comes from a
  // trusted source. If this is not the case, use the
  // `data_decoder::JsonSanitizer` to rinse the data first.
  //
  // Creates an instance that can be serialized for IPC. This uses additional
  // memory to store the raw animation data.
  static scoped_refptr<SkottieWrapper> UnsafeCreateSerializable(
      std::vector<uint8_t> data);

  // Creates a non serializable instance of the class. This uses less memory.
  static scoped_refptr<SkottieWrapper> UnsafeCreateNonSerializable(
      base::span<const uint8_t> data);

  SkottieWrapper(const SkottieWrapper&) = delete;
  SkottieWrapper& operator=(const SkottieWrapper&) = delete;

  // Returns true if this object contains a valid skottie animation.
  virtual bool is_valid() const = 0;

  // Returns the set of all image assets in the animation and their
  // corresponding metadata. The returned map is effectively immutable; it
  // does not change during SkottieWrapper's lifetime.
  virtual const SkottieResourceMetadataMap& GetImageAssetMetadata() const = 0;

  // Returns the set of text nodes in the animation. There shall be an entry in
  // GetCurrentTextPropertyValues() for each returned node. The returned set is
  // immutable and does not change during SkottieWrapper's lifetime.
  virtual const base::flat_set<std::string>& GetTextNodeNames() const = 0;

  // Returns a map from hashed animation node name to its current property
  // value in the animation (see SkottieProperty.h). Some properties' values
  // can be updated via its corresponding argument in Draw().
  virtual SkottieTextPropertyValueMap GetCurrentTextPropertyValues() const = 0;
  virtual SkottieTransformPropertyValueMap GetCurrentTransformPropertyValues()
      const = 0;
  virtual SkottieColorMap GetCurrentColorPropertyValues() const = 0;

  // Returns all markers present in the animation. The returned list is
  // immutable and does not change during SkottieWrapper's lifetime.
  virtual const std::vector<SkottieMarker>& GetAllMarkers() const = 0;

  // FrameDataCallback is implemented by the caller and invoked
  // synchronously during calls to Seek() and Draw(). The callback is used by
  // SkottieWrapper to fetch the corresponding image for each asset that is
  // present in the frame with the desired timestamp. It is invoked once for
  // each asset. A null callback may be passed to Seek() and Draw() if the
  // animation is known to not have any image assets.
  enum class FrameDataFetchResult {
    // A new image is available for the given asset, and the callback's output
    // parameters have been filled with the new frame data.
    kNewDataAvailable,
    // The callback's output parameters have not been filled and will be
    // ignored by SkottieWrapper. In this case, SkottieWrapper will reuse the
    // frame data that was most recently provided for the given asset (it caches
    // this internally). Note it is acceptable to set |image_out| to a null
    // SkImage; Skottie will simply skip the image asset while rendering the
    // rest of the frame.
    kNoUpdate,
  };
  // The callback's implementation must synchronously fill the output
  // arguments. |asset_id| is guaranteed to be a valid asset that's present
  // in GetImageAssetMetadata(). See skresources::ImageAsset::getFrame() for
  // the semantics of |t|.
  using FrameDataCallback = base::RepeatingCallback<FrameDataFetchResult(
      SkottieResourceIdHash asset_id,
      float t,
      sk_sp<SkImage>& image_out,
      SkSamplingOptions& sampling_out)>;

  // Seeks to the normalized time instant |t|, but does not render. This method
  // is thread safe.
  virtual void Seek(float t, FrameDataCallback frame_data_cb) = 0;
  // Variant with null FrameDataCallback() if the animation does not have image
  // assets.
  void Seek(float t);

  // A thread safe call that will draw an image with bounds |rect| for the
  // frame at normalized time instant |t| onto the |canvas|.
  //
  // |text_map| may be an incremental update and only contain a subset of the
  // text nodes in the animation. If a text node is absent from |text_map|, it
  // will maintain the same contents as the previous call to Draw().
  virtual void Draw(SkCanvas* canvas,
                    float t,
                    const SkRect& rect,
                    FrameDataCallback frame_data_cb,
                    const SkottieColorMap& color_map,
                    const SkottieTextPropertyValueMap& text_map) = 0;

  virtual float duration() const = 0;
  virtual SkSize size() const = 0;

  virtual base::span<const uint8_t> raw_data() const = 0;
  virtual uint32_t id() const = 0;

 protected:
  SkottieWrapper() = default;
  virtual ~SkottieWrapper() = default;

 private:
  friend class base::RefCountedThreadSafe<SkottieWrapper>;
};

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_WRAPPER_H_
