// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TOUCH_ACTION_REGION_H_
#define CC_LAYERS_TOUCH_ACTION_REGION_H_

#include "base/containers/flat_map.h"
#include "cc/base/region.h"
#include "cc/cc_export.h"
#include "cc/input/touch_action.h"

namespace cc {

class CC_EXPORT TouchActionRegion {
 public:
  TouchActionRegion();
  TouchActionRegion(const TouchActionRegion& touch_action_region);
  TouchActionRegion(TouchActionRegion&& touch_action_region);
  ~TouchActionRegion();

  void Union(TouchAction, const gfx::Rect&);
  // Return all regions with any touch action.
  Region GetAllRegions() const;
  const Region& GetRegionForTouchAction(TouchAction) const;

  bool IsEmpty() const { return map_.empty(); }

  // Returns the touch actions that we are sure will be allowed at the point
  // by finding the intersection of all touch actions whose regions contain the
  // given point. If the map is empty, |TouchAction::kAuto| is returned since no
  // touch actions have been explicitly defined and the default touch action
  // is auto.
  TouchAction GetAllowedTouchAction(const gfx::Point&) const;
  TouchActionRegion& operator=(const TouchActionRegion& other);
  TouchActionRegion& operator=(TouchActionRegion&& other);
  bool operator==(const TouchActionRegion& other) const;

 private:
  base::flat_map<TouchAction, Region> map_;
};

}  // namespace cc

#endif  // CC_LAYERS_TOUCH_ACTION_REGION_H_
