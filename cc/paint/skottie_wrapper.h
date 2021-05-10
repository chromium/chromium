// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_WRAPPER_H_
#define CC_PAINT_SKOTTIE_WRAPPER_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/modules/skottie/include/Skottie.h"

class SkCanvas;
struct SkRect;

namespace cc {

// A wrapper over Skia's Skottie object that can be shared by multiple
// SkiaVectorAnimation objects. This class is thread safe when performing a draw
// on an SkCanvas.
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

  // Returns true if the |animation_| object initialized is a valid skottie
  // animation.
  bool is_valid() const { return !!animation_; }

  // A thread safe call that will draw an image with bounds |rect| for the
  // frame at normalized time instant |t| onto the |canvas|.
  void Draw(SkCanvas* canvas, float t, const SkRect& rect);

  float duration() const { return animation_->duration(); }
  SkSize size() const { return animation_->size(); }

  base::span<const uint8_t> raw_data() const;
  uint32_t id() const { return id_; }

 private:
  friend class base::RefCountedThreadSafe<SkottieWrapper>;

  explicit SkottieWrapper(base::span<const uint8_t> data);
  explicit SkottieWrapper(std::vector<uint8_t> data);

  ~SkottieWrapper();

  base::Lock lock_;
  sk_sp<skottie::Animation> animation_;

  // The raw byte data is stored for serialization across OOP-R. This is only
  // valid if serialization was enabled at construction.
  const std::vector<uint8_t> raw_data_;

  // Unique id generated for a given animation. This will be unique per
  // animation file. 2 animation objects from the same source file will have the
  // same value.
  const uint32_t id_;
};

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_WRAPPER_H_
