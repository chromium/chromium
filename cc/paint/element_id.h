// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_ELEMENT_ID_H_
#define CC_PAINT_ELEMENT_ID_H_

#include <stddef.h>

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>

#include "base/hash/hash.h"
#include "cc/paint/paint_export.h"

namespace base {
class Value;
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

using ElementIdType = uint64_t;

// Element ids are chosen by cc's clients and can be used as a stable identifier
// across updates.
//
// Historically, the layer tree stored all compositing data but this has been
// refactored over time into auxilliary structures such as property trees.
//
// In composited scrolling, Layers directly reference scroll tree nodes
// (Layer::scroll_tree_index) but scroll tree nodes are being refactored to
// reference stable element ids instead of layers. Scroll property nodes have
// unique element ids that blink creates from scrollable areas (though this is
// opaque to the compositor). This refactoring of scroll nodes keeping a
// scrolling element id instead of a scrolling layer id allows for more general
// compositing where, for example, multiple layers scroll with one scroll node.
//
// The animation system (see ElementAnimations) is another auxilliary structure
// to the layer tree and uses element ids as a stable identifier for animation
// targets. A Layer's element id can change over the Layer's lifetime because
// non-default ElementIds are only set during an animation's lifetime.
struct CC_PAINT_EXPORT ElementId {
  explicit ElementId(ElementIdType id) : id_(id) {
    DCHECK_NE(id, kInvalidElementId);
  }

  ElementId() : id_(kInvalidElementId) {}

  bool operator==(const ElementId& o) const { return id_ == o.id_; }
  bool operator!=(const ElementId& o) const { return !(*this == o); }
  bool operator<(const ElementId& o) const { return id_ < o.id_; }

  // Returns true if the ElementId has been initialized with a valid id.
  explicit operator bool() const { return !!id_; }

  void AddToTracedValue(base::trace_event::TracedValue* res) const;
  std::unique_ptr<base::Value> AsValue() const;

  ElementIdType GetStableId() const;

  std::string ToString() const;

 private:
  friend struct ElementIdHash;
  static const ElementIdType kInvalidElementId;

  // The compositor treats this as an opaque handle and should not know how to
  // interpret these bits. Non-blink cc clients typically operate in terms of
  // layers and may set this value to match the client's layer id.
  ElementIdType id_;
};

ElementId CC_PAINT_EXPORT LayerIdToElementIdForTesting(int layer_id);

struct CC_PAINT_EXPORT ElementIdHash {
  size_t operator()(ElementId key) const;
};

// Stream operator so ElementId can be used in assertion statements.
CC_PAINT_EXPORT std::ostream& operator<<(std::ostream& out,
                                         const ElementId& id);

}  // namespace cc

#endif  // CC_PAINT_ELEMENT_ID_H_
