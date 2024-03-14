// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_CLIENT_H_
#define ASH_BIRCH_BIRCH_CLIENT_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

namespace ash {

class BirchDataProvider;

// Interface to communicate with the birch keyed service.
class ASH_EXPORT BirchClient {
 public:
  virtual BirchDataProvider* GetCalendarProvider() = 0;
  virtual BirchDataProvider* GetFileSuggestProvider() = 0;
  virtual BirchDataProvider* GetRecentTabsProvider() = 0;
  virtual BirchDataProvider* GetReleaseNotesProvider() = 0;

  // Waits for refresh tokens to be loaded then calls `callback`. Calls
  // `callback` immediately if tokens are already loaded. Only one waiter
  // at a time is supported.
  virtual void WaitForRefreshTokens(base::OnceClosure callback) = 0;

  virtual ~BirchClient() = default;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_CLIENT_H_
