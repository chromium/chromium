// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DEFERRED_VALUES_H_
#define CC_PAINT_DEFERRED_VALUES_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/animation_curve_proxy.h"
#include "cc/paint/deferred_property_key.h"
#include "cc/paint/deferred_value_map.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

// Generates a value that paint records by key, and that a compositor animation
// updates without a new commit. The animation sets the progress, and the value
// is only sampled when needed.
class CC_PAINT_EXPORT DeferredValueGenerator {
 public:
  enum class Type { kColor };

  DeferredValueGenerator(const DeferredValueGenerator&) = delete;
  DeferredValueGenerator& operator=(const DeferredValueGenerator&) = delete;
  virtual ~DeferredValueGenerator();

  virtual Type GetType() const = 0;

  // Sets the progress, or clears it when the animation no longer drives the
  // value. Returns true if the progress changed.
  bool SetProgress(std::optional<float> progress);
  bool NeedsRecompute() const { return generation_ != last_generation_; }

  // Samples the value if the progress changed since the last sample.
  void RecomputeValue();

  // Returns true if a sampled value is available. Otherwise, raster uses the
  // fallback value that paint recorded.
  bool has_value() const { return has_value_; }

 protected:
  DeferredValueGenerator();

  virtual void Sample() = 0;
  float progress() const { return *progress_; }
  void set_has_value(bool has_value) { has_value_ = has_value; }

 private:
  std::optional<float> progress_;
  uint64_t generation_ = 0;
  uint64_t last_generation_ = 0;
  bool has_value_ = false;
};

// Evaluates any color-valued deferred property (e.g. `color`,
// `background-color`) from an `AnimationCurveProxy<SkColor4f>`.
class CC_PAINT_EXPORT DeferredColorValueGenerator final
    : public DeferredValueGenerator {
 public:
  explicit DeferredColorValueGenerator(
      scoped_refptr<const AnimationCurveProxy<SkColor4f>> curve);

  static const DeferredColorValueGenerator* From(
      const DeferredValueGenerator* generator) {
    return generator && generator->GetType() == Type::kColor
               ? static_cast<const DeferredColorValueGenerator*>(generator)
               : nullptr;
  }

  // DeferredValueGenerator:
  ~DeferredColorValueGenerator() override;
  Type GetType() const override;

  const AnimationCurveProxy<SkColor4f>* curve() const { return curve_.get(); }
  SkColor4f GetColor() const { return value_; }
  void SetColor(SkColor4f color) {
    value_ = color;
    set_has_value(true);
  }

 protected:
  // DeferredValueGenerator:
  void Sample() override;

 private:
  scoped_refptr<const AnimationCurveProxy<SkColor4f>> curve_;
  SkColor4f value_ = SkColors::kTransparent;
};

// Maps each deferred property to its value generator.
using DeferredValues = base::flat_map<DeferredPropertyKey,
                                      std::unique_ptr<DeferredValueGenerator>>;

// Samples `values` whose progress changed, and returns the map of resolved
// values. Keys without a value are omitted so raster keeps the fallback.
CC_PAINT_EXPORT DeferredValueMap
GenerateDeferredValueMap(DeferredValues& values);

}  // namespace cc

#endif  // CC_PAINT_DEFERRED_VALUES_H_
