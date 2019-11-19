// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/consistency_golden_loader.h"

#include "base/files/dir_reader_posix.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"

namespace chromeos {
namespace time_limit_consistency {
namespace {

base::FilePath GetGoldensPath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);

  return path.Append(
      FILE_PATH_LITERAL("chrome/browser/chromeos/child_accounts/"
                        "time_limit_consistency_test/goldens"));
}

}  // namespace

std::vector<GoldenParam> LoadGoldenCases() {
  return LoadGoldenCasesFromPath(GetGoldensPath());
}

std::vector<GoldenParam> LoadGoldenCasesFromPath(
    const base::FilePath& directory_path) {
  std::vector<GoldenParam> golden_params_list;
  base::DirReaderPosix dir_reader(directory_path.value().c_str());

  while (dir_reader.Next()) {
    if (!base::EndsWith(dir_reader.name(), ".textproto",
                        base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }

    ConsistencyGolden golden_suite;
    base::File golden_file(directory_path.Append(dir_reader.name()),
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
    google::protobuf::io::FileInputStream stream(golden_file.GetPlatformFile());
    google::protobuf::TextFormat::Parse(&stream, &golden_suite);

    // Ignore suites that don't include CHROME_OS as a supported platform.
    bool chromeos_supported =
        std::count(golden_suite.supported_platforms().begin(),
                   golden_suite.supported_platforms().end(), CHROME_OS) > 0;
    if (!chromeos_supported)
      continue;

    std::string suite_name = dir_reader.name();
    base::ReplaceFirstSubstringAfterOffset(&suite_name, 0, ".textproto", "");
    for (int i = 0; i < golden_suite.cases_size(); i++) {
      golden_params_list.push_back(
          GoldenParam({suite_name, i, golden_suite.cases(i)}));
    }
  }

  return golden_params_list;
}

}  // namespace time_limit_consistency
}  // namespace chromeos
