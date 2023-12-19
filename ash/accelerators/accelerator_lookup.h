// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_LOOKUP_H_
#define ASH_ACCELERATORS_ACCELERATOR_LOOKUP_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {

// AceleratorLookup is a slim singleton class that is used to lookup
// accelerators for a particular action.
class ASH_EXPORT AcceleratorLookup {
 public:
  explicit AcceleratorLookup(
      raw_ptr<AcceleratorConfiguration> ash_accelerators);
  ~AcceleratorLookup();
  AcceleratorLookup(const AcceleratorLookup&) = delete;
  AcceleratorLookup& operator=(const AcceleratorLookup&) = delete;

  struct AcceleratorDetails {
    // The base accelerator.
    ui::Accelerator accelerator;
    // The regionalized string representation of the activation key.
    std::u16string key_display;
  };

  // Returns a list of accelerator details for `action`.
  std::vector<AcceleratorDetails> GetAcceleratorsForAction(
      uint32_t action) const;

 private:
  raw_ptr<AcceleratorConfiguration> ash_accelerator_configuration_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_LOOKUP_H_
