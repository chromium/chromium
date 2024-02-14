// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_CALENDAR_PROVIDER_H_
#define ASH_BIRCH_BIRCH_CALENDAR_PROVIDER_H_

#include "ash/birch/birch_client.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class BirchModel;

// Provides calendar event data to the birch model.
class BirchCalendarProvider : public BirchClient {
 public:
  explicit BirchCalendarProvider(BirchModel* birch_model);
  BirchCalendarProvider(const BirchCalendarProvider&) = delete;
  BirchCalendarProvider& operator=(const BirchCalendarProvider&) = delete;
  ~BirchCalendarProvider() override;

  // BirchClient:
  void RequestBirchDataFetch() override;

 private:
  // Called in response to a calendar info request. Updates the birch model with
  // data from the calendar events.
  void OnCalendarInfoFetched();

  const raw_ptr<BirchModel> birch_model_;

  base::WeakPtrFactory<BirchCalendarProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_CALENDAR_PROVIDER_H_
