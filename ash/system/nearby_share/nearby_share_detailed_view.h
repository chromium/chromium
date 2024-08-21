// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_DETAILED_VIEW_H_
#define ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_DETAILED_VIEW_H_

#include <memory>

#include "ash/ash_export.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class DetailedViewDelegate;

// Defines the interface used to interact with the detailed Quick Share
// (formerly Nearby Share) page.
class ASH_EXPORT NearbyShareDetailedView {
 public:
  class Factory {
   public:
    Factory(const Factory&) = delete;
    const Factory& operator=(const Factory&) = delete;
    virtual ~Factory() = default;

    static std::unique_ptr<NearbyShareDetailedView> Create(
        DetailedViewDelegate* detailed_view_delegate);

   protected:
    Factory() = default;
  };

  NearbyShareDetailedView(const NearbyShareDetailedView&) = delete;
  NearbyShareDetailedView& operator=(const NearbyShareDetailedView&) = delete;
  virtual ~NearbyShareDetailedView() = default;

  virtual views::View* GetAsView() = 0;

 protected:
  NearbyShareDetailedView() = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_DETAILED_VIEW_H_
