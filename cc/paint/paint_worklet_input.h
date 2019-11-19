// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_WORKLET_INPUT_H_
#define CC_PAINT_PAINT_WORKLET_INPUT_H_

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cc/paint/element_id.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {

class PaintOpBuffer;
using PaintRecord = PaintOpBuffer;

class CC_PAINT_EXPORT PaintWorkletInput
    : public base::RefCountedThreadSafe<PaintWorkletInput> {
 public:
  // Uniquely identifies a property from the animation system, so that a
  // PaintWorkletInput can specify the properties it depends on to be painted
  // (and for which it must be repainted if their values change).
  //
  // PropertyKey is designed to support both native and custom properties. The
  // same ElementId will be produced for all custom properties for a given
  // element. As such we require the custom property name as an additional key
  // to uniquely identify custom properties.
  using PropertyKey = std::pair<std::string, ElementId>;

  // A structure that can hold either a float or color type value, depending
  // on the type of custom property.  Only one of |float_val| and |color_val|
  // should hold value at any time; if neither hold value then we should not use
  // this value.
  // This structure is needed so that PaintWorkletProxyClient::Paint can handle
  // both color and float type custom property values at the same time.
  struct CC_PAINT_EXPORT PropertyValue {
    PropertyValue();
    explicit PropertyValue(float value);
    explicit PropertyValue(SkColor value);
    PropertyValue(const PropertyValue&);
    ~PropertyValue();
    bool has_value() const;
    void reset();
    base::Optional<float> float_value;
    base::Optional<SkColor> color_value;
  };

  virtual gfx::SizeF GetSize() const = 0;

  virtual int WorkletId() const = 0;

  // Return the list of properties used as an input by this PaintWorkletInput.
  // The values for these properties must be provided when dispatching a worklet
  // job for this input.
  using PropertyKeys = std::vector<PropertyKey>;
  virtual const PropertyKeys& GetPropertyKeys() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<PaintWorkletInput>;
  virtual ~PaintWorkletInput() = default;
};

// PaintWorkletRecordMap ties the input for a PaintWorklet (PaintWorkletInput)
// to the painted output (a PaintRecord). It also stores the PaintImage::Id for
// the PaintWorklet to enable efficient invalidation of dirty PaintWorklets.
using PaintWorkletRecordMap =
    base::flat_map<scoped_refptr<const PaintWorkletInput>,
                   std::pair<PaintImage::Id, sk_sp<PaintRecord>>>;

}  // namespace cc

#endif  // CC_PAINT_PAINT_WORKLET_INPUT_H_
