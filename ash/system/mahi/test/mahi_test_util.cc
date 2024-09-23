// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/test/mahi_test_util.h"

#include <utility>
#include <vector>

#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

namespace ash::mahi_test_util {

namespace {

// Constants -------------------------------------------------------------------

const std::vector<chromeos::MahiOutline> kFakeOutlines(
    {chromeos::MahiOutline(/*id=*/1, u"Outline 1"),
     chromeos::MahiOutline(/*id=*/2, u"Outline 2"),
     chromeos::MahiOutline(/*id=*/3, u"Outline 3"),
     chromeos::MahiOutline(/*id=*/4, u"Outline 4"),
     chromeos::MahiOutline(/*id=*/5, u"Outline 5")});

}  // namespace

const std::vector<chromeos::MahiOutline>& GetDefaultFakeOutlines() {
  return kFakeOutlines;
}

void ReturnDefaultOutlines(
    chromeos::MahiManager::MahiOutlinesCallback callback) {
  std::move(callback).Run(kFakeOutlines,
                          chromeos::MahiResponseStatus::kSuccess);
}

}  // namespace ash::mahi_test_util
