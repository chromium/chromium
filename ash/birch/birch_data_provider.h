// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_DATA_PROVIDER_H_
#define ASH_BIRCH_BIRCH_DATA_PROVIDER_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"

namespace ash {

// Interface for a Birch data source.
class ASH_EXPORT BirchDataProvider {
 public:
  BirchDataProvider(const BirchDataProvider&) = delete;
  BirchDataProvider& operator=(const BirchDataProvider&) = delete;
  virtual ~BirchDataProvider();

  // Requests that the data provider start fetching data. The provider should
  // add the data to the BirchModel after the fetch completes.
  virtual void RequestBirchDataFetch() = 0;

  // Sets a callback for the data provider to notify its changes.
  void SetDataProviderChangedCallback(base::RepeatingClosure callback);
  void ResetDataProviderChangedCallback();

 protected:
  BirchDataProvider();

  // Runs the data provider changed callback.
  void NotifyDataProviderChanged();

 private:
  // The callback which runs when the data provider wants to notify its change.
  base::RepeatingClosure data_provider_changed_callback_;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_DATA_PROVIDER_H_
