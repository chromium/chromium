// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A utility for loading golden files to be used by the time limit processor
// consistency tests.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_CONSISTENCY_TEST_CONSISTENCY_GOLDEN_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_CONSISTENCY_TEST_CONSISTENCY_GOLDEN_LOADER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/goldens/consistency_golden.pb.h"

namespace chromeos {
namespace time_limit_consistency {

// Holds information for one golden case and metadata used to generate the name
// for its test case (i.e. the name of the golden file it belongs and its index
// inside it).
struct GoldenParam {
  const std::string suite_name;
  const int index;
  const ConsistencyGoldenCase golden_case;
};

// Loads all cases from all available golden files into a list of GoldenParams.
std::vector<GoldenParam> LoadGoldenCases();

// Loads all cases from all golden files from a given path into a list of
// GoldenParams. LoadGoldenCases() uses this function under the hood. Should not
// be called directly except for testing the golden loader itself.
std::vector<GoldenParam> LoadGoldenCasesFromPath(
    const base::FilePath& directory_path);

}  // namespace time_limit_consistency
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_CONSISTENCY_TEST_CONSISTENCY_GOLDEN_LOADER_H_
