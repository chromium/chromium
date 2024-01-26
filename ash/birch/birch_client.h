// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_CLIENT_H_
#define ASH_BIRCH_BIRCH_CLIENT_H_

#include "ash/ash_export.h"

namespace ash {

// Interface to communicate with the birch keyed service.
class ASH_EXPORT BirchClient {
 public:
  // Request that the birch keyed service start fetching data.
  virtual void RequestBirchDataFetch() = 0;

  virtual ~BirchClient() = default;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_CLIENT_H_
