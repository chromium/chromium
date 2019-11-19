// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/element_id.h"

#include <inttypes.h>
#include <limits>
#include <ostream>

#include "base/strings/stringprintf.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"

namespace cc {

const ElementIdType ElementId::kInvalidElementId = 0;

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

std::unique_ptr<base::Value> ElementId::AsValue() const {
  std::unique_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  res->SetInteger("id_", id_);
  return std::move(res);
}

size_t ElementIdHash::operator()(ElementId key) const {
  return std::hash<int>()(key.id_);
}

std::ostream& operator<<(std::ostream& out, const ElementId& id) {
  return out << id.ToString();
}

}  // namespace cc
