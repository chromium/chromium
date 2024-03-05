// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_DATA_PROVIDER_H_
#define ASH_BIRCH_BIRCH_DATA_PROVIDER_H_

#include "ash/ash_export.h"

namespace ash {

// Interface for a Birch data source.
class ASH_EXPORT BirchDataProvider {
 public:
  // Requests that the data provider start fetching data. The provider should
  // add the data to the BirchModel after the fetch completes.
  virtual void RequestBirchDataFetch() = 0;

  virtual ~BirchDataProvider() = default;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_DATA_PROVIDER_H_
