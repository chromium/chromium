// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_WRAPPER_H_
#define CC_PAINT_SKOTTIE_WRAPPER_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/skottie_resource_metadata.h"
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
  // Creates an instance that can be serialized for IPC. This uses additional
  // memory to store the raw animation data.
  static scoped_refptr<SkottieWrapper> CreateSerializable(
      std::vector<uint8_t> data);

  // Creates a non serializable instance of the class. This uses less memory.
  static scoped_refptr<SkottieWrapper> CreateNonSerializable(
      base::span<const uint8_t> data);

  SkottieWrapper(const SkottieWrapper&) = delete;
  SkottieWrapper& operator=(const SkottieWrapper&) = delete;

  // Returns true if this object contains a valid skottie animation.
  virtual bool is_valid() const = 0;

  // Returns the set of all image assets in the animation and their
  // corresponding metadata. The returned map is effectively immutable; it
  // does not change during SkottieWrapper's lifetime.
  virtual const SkottieResourceMetadataMap& GetImageAssetMetadata() const = 0;

  // FrameDataCallback is implemented by the caller and invoked
  // synchronously during calls to Seek() and Draw(). The callback is used by
  // SkottieWrapper to fetch the corresponding image for each asset that is
  // present in the frame with the desired timestamp. It is invoked once for
  // each asset. A null callback may be passed to Seek() and Draw() if the
  // animation is known to not have any image assets.
  enum class FrameDataFetchResult {
    // A new image is available for the given asset, and the callback's output
    // parameters have been filled with the new frame data.
    NEW_DATA_AVAILABLE,
    // The callback's output parameters have not been filled and will be
    // ignored by SkottieWrapper. In this case, SkottieWrapper will reuse the
    // frame data that was most recently provided for the given asset (it caches
    // this internally). If no frame data has ever been provided for this asset,
    // a null image will be passed to Skottie's Animation during Seek(); this
    // is acceptable if there's no rendering.
    NO_UPDATE,
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
  virtual void Draw(SkCanvas* canvas,
                    float t,
                    const SkRect& rect,
                    FrameDataCallback frame_data_cb) = 0;
  // Variant with null FrameDataCallback() if the animation does not have image
  // assets.
  void Draw(SkCanvas* canvas, float t, const SkRect& rect);

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
