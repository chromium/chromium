// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_TEST_MAHI_TEST_UTIL_H_
#define ASH_SYSTEM_MAHI_TEST_MAHI_TEST_UTIL_H_

#include <vector>

#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

namespace ash::mahi_test_util {

// Returns a default outline array for testing.
const std::vector<chromeos::MahiOutline>& GetDefaultFakeOutlines();

// Runs `callback` to return the default outlines successfully.
void ReturnDefaultOutlines(
    chromeos::MahiManager::MahiOutlinesCallback callback);

}  // namespace ash::mahi_test_util

#endif  // ASH_SYSTEM_MAHI_TEST_MAHI_TEST_UTIL_H_
