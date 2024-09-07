// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_ELEMENT_ID_H_
#define CC_PAINT_ELEMENT_ID_H_

#include <stddef.h>

#include <cstdint>
#include <iosfwd>
#include <limits>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "cc/paint/paint_export.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

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
  using InternalValue = uint64_t;

  static constexpr InternalValue kInvalidElementId = 0;
  static constexpr InternalValue kDeletedElementId =
      std::numeric_limits<InternalValue>::max();

  // Constructs an invalid element id.
  constexpr ElementId() : id_(kInvalidElementId) {}

  explicit constexpr ElementId(InternalValue id) : id_(id) {
    DCHECK_NE(id, kInvalidElementId);
    DCHECK_NE(id, kDeletedElementId);
  }

  static constexpr ElementId DeletedValue() {
    ElementId value;
    value.id_ = kDeletedElementId;
    return value;
  }

  bool operator==(const ElementId& o) const { return id_ == o.id_; }
  bool operator!=(const ElementId& o) const { return !(*this == o); }
  bool operator<(const ElementId& o) const { return id_ < o.id_; }

  // Returns true if the ElementId has been initialized with a valid id.
  explicit constexpr operator bool() const { return IsValidInternalValue(id_); }

  void AddToTracedValue(base::trace_event::TracedValue* res) const;

  std::string ToString() const;

  // Returns the internal id. Use this function with caution not to break
  // opaqueness of the id.
  InternalValue GetInternalValue() const { return id_; }
  static constexpr bool IsValidInternalValue(InternalValue value) {
    return value != kInvalidElementId && value != kDeletedElementId;
  }

 private:
  friend struct ElementIdHash;

  // The compositor treats this as an opaque handle and should not know how to
  // interpret these bits. Non-blink cc clients typically operate in terms of
  // layers and may set this value to match the client's layer id.
  InternalValue id_;
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
