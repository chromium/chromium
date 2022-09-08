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

const ElementIdType ElementId::kInvalidElementId = 0;
const ElementIdType ElementId::kReservedElementId =
    std::numeric_limits<ElementIdType>::max();

// static
bool ElementId::IsValid(ElementIdType id) {
  return id != kInvalidElementId;
}

ElementId LayerIdToElementIdForTesting(int layer_id) {
  return ElementId(std::numeric_limits<int>::max() - layer_id);
}

void ElementId::AddToTracedValue(base::trace_event::TracedValue* res) const {
  res->BeginDictionary("element_id");
  res->SetInteger("id_", id_);
  res->EndDictionary();
}

ElementIdType ElementId::GetStableId() const {
  return id_;
}

std::string ElementId::ToString() const {
  return base::StringPrintf("(%" PRIu64 ")", id_);
}

size_t ElementIdHash::operator()(ElementId key) const {
  return std::hash<int>()(key.id_);
}

std::ostream& operator<<(std::ostream& out, const ElementId& id) {
  return out << id.ToString();
}

}  // namespace cc
