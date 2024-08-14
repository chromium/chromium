// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/element_id.h"

#include <inttypes.h>

#include <limits>
#include <ostream>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/trace_event/traced_value.h"

namespace cc {

ElementId LayerIdToElementIdForTesting(int layer_id) {
  // This intentionally returns an ElementId that is different from layer_id
  // and is unlikely to conflict with other ElementIds. This ensures testing of
  // our code not to depend on that the UI compositor uses ElementId(layer_id)
  // as the element id of a layer.
  return ElementId(std::numeric_limits<int>::max() - layer_id);
}

void ElementId::AddToTracedValue(base::trace_event::TracedValue* res) const {
  res->BeginDictionary("element_id");
  res->SetInteger("id_", id_);
  res->EndDictionary();
}

std::string ElementId::ToString() const {
  return base::StringPrintf("(%" PRIu64 ")", id_);
}

size_t ElementIdHash::operator()(ElementId key) const {
  return std::hash<ElementId::InternalValue>()(key.id_);
}

std::ostream& operator<<(std::ostream& out, const ElementId& id) {
  return out << id.ToString();
}

}  // namespace cc
