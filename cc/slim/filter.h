// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_FILTER_H_
#define CC_SLIM_FILTER_H_

#include "base/component_export.h"

namespace cc::slim {

// Filters modeled after css filter property.
// Note a layer with filter is more expensive than two layers layers with
// opacity blending, so always prefer to use additional layers if possible.
class COMPONENT_EXPORT(CC_SLIM) Filter {
 public:
  enum Type {
    kBrightness,
    kSaturation,
  };

  Filter(const Filter&);
  Filter& operator=(const Filter&);
  ~Filter();

  // `amount` is on a scale from 0.f to 1.f, though amount is allowed to
  // exceed 1.f.
  static Filter CreateBrightness(float amount) { return {kBrightness, amount}; }
  static Filter CreateSaturation(float amount) { return {kSaturation, amount}; }

  bool operator==(const Filter& other) const;
  bool operator!=(const Filter& other) const { return !(*this == other); }

  Type type() const { return type_; }
  float amount() const { return amount_; }

 private:
  Filter(Type type, float amount);

  Type type_ = kBrightness;
  float amount_ = 0.0f;
};

}  // namespace cc::slim

#endif  // CC_SLIM_FILTER_H_
