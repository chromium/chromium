// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PATH_EFFECT_H_
#define CC_PAINT_PATH_EFFECT_H_

#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkPathEffect;

namespace cc {

class PaintOpWriter;
class PaintOpReader;

class CC_PAINT_EXPORT PathEffect : public SkRefCnt {
 public:
  PathEffect(const PathEffect&) = delete;
  PathEffect& operator=(const PathEffect&) = delete;

  static sk_sp<PathEffect> MakeDash(const float intervals[],
                                    int count,
                                    float phase);
  static sk_sp<PathEffect> MakeCorner(float radius);

  bool EqualsForTesting(const PathEffect& other) const;

  // If this is a Dash PathEffect, how many intervals are in the dash pattern?
  // Returns 0 for all other kinds of PathEffect.
  virtual size_t dash_interval_count() const;

 protected:
  friend class PaintFlags;
  friend class PaintOpReader;
  friend class PaintOpWriter;

  enum class Type {
    // kNull is for serialization purposes only, to indicate a null path effect
    // in a containing object (e.g. PaintFlags).
    kNull,
    kDash,
    kCorner,
    kMaxValue = kCorner,
  };

  virtual sk_sp<SkPathEffect> GetSkPathEffect() const = 0;

  explicit PathEffect(Type type);
  // These functions don't handle type_. It's handled in PaintOpWriter/Reader.
  virtual size_t SerializedDataSize() const = 0;
  virtual void SerializeData(PaintOpWriter& writer) const = 0;
  static sk_sp<PathEffect> Deserialize(PaintOpReader& reader, Type type);

  Type type_;
};

}  // namespace cc

#endif  // CC_PAINT_PATH_EFFECT_H_
