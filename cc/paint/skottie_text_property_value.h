// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_TEXT_PROPERTY_VALUE_H_
#define CC_PAINT_SKOTTIE_TEXT_PROPERTY_VALUE_H_

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

// Contains a subset of the fields in skottie::TextPropertyValue that the caller
// may want to override when rendering the animation. The primary field of
// course is the text itself, but other fields may be added to this class as
// desired. All skottie::TextPropertyValue fields not present in this class will
// ultimately assume the same values as those baked into the Lottie file when
// rendered.
//
// This class is intentionally cheap to copy.
class CC_PAINT_EXPORT SkottieTextPropertyValue {
 public:
  SkottieTextPropertyValue(std::string text, gfx::RectF box);
  SkottieTextPropertyValue(const SkottieTextPropertyValue& other);
  SkottieTextPropertyValue& operator=(const SkottieTextPropertyValue& other);
  ~SkottieTextPropertyValue();

  bool operator==(const SkottieTextPropertyValue& other) const;
  bool operator!=(const SkottieTextPropertyValue& other) const;

  void SetText(std::string text);
  const std::string& text() const { return text_->as_string(); }

  void set_box(gfx::RectF box) { box_ = std::move(box); }
  const gfx::RectF& box() const { return box_; }

 private:
  // Make the text ref-counted to eliminate as many deep copies as possible when
  // this class is passed through the rendering pipeline. Note the text's string
  // content is never mutated once it's set, eliminating the chance of any race
  // conditions.
  scoped_refptr<base::RefCountedString> text_;
  // For fast comparison operator.
  size_t text_hash_ = 0;
  gfx::RectF box_;
};

// Node name in the Lottie file (hashed) to corresponding
// SkottieTextPropertyValue.
using SkottieTextPropertyValueMap =
    base::flat_map<SkottieResourceIdHash, SkottieTextPropertyValue>;

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_TEXT_PROPERTY_VALUE_H_
