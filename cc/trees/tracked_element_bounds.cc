// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/tracked_element_bounds.h"

#include <sstream>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "base/strings/string_util.h"

namespace cc {

std::string TrackedElementBoundsToString(const TrackedElementBounds& bounds) {
  std::vector<std::string> element_strings;
  for (const auto& [tracker_id, data] : bounds) {
    std::string s = "tracker_id: " + tracker_id.ToString() +
                    ", visible_bounds: " + data.visible_bounds.ToString();
    element_strings.push_back(s);
  }
  return base::JoinString(element_strings, "; ");
}

const TrackedElementBounds& TrackedElementBoundsEmpty() {
  static const base::NoDestructor<TrackedElementBounds> empty_bounds;
  return *empty_bounds;
}

}  // namespace cc
