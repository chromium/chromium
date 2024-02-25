// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_FEATURE_TILE_H_
#define ASH_SYSTEM_NETWORK_NETWORK_FEATURE_TILE_H_

#include "ash/ash_export.h"
#include "ash/system/unified/feature_tile.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class Button;

// This class is used to notify the view's theme changes to its delegates.
class ASH_EXPORT NetworkFeatureTile : public FeatureTile {
  METADATA_HEADER(NetworkFeatureTile, FeatureTile)

 public:
  class Delegate {
   public:
    virtual void OnFeatureTileThemeChanged() = 0;
  };

  NetworkFeatureTile(Delegate* delegate,
                     base::RepeatingCallback<void()> callback);
  NetworkFeatureTile(const NetworkFeatureTile&) = delete;
  NetworkFeatureTile& operator=(const NetworkFeatureTile&) = delete;
  ~NetworkFeatureTile() override;

 private:
  // views::Button:
  void OnThemeChanged() override;

  const raw_ptr<Delegate, DanglingUntriaged> delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_FEATURE_TILE_H_
