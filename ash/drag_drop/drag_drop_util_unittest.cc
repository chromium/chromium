// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_util.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#include "url/gurl.h"

namespace ash::drag_drop {
namespace {

// Helpers ---------------------------------------------------------------------

// Returns a created `ui::OSExchangeData` instance with optional file paths
// stored (a) at the file system sources storage location used by the Files app,
// and/or (b) at the filenames storage location, and an optional `source_url`.
std::unique_ptr<ui::OSExchangeData> CreateOSExchangeData(
    const std::u16string& file_system_sources,
    const std::vector<base::FilePath>& filenames,
    std::optional<GURL> source_url = std::nullopt) {
  auto data = std::make_unique<ui::OSExchangeData>(
      std::make_unique<ui::OSExchangeDataProviderNonBacked>());

  if (source_url.has_value()) {
    data->SetSource(std::make_unique<ui::DataTransferEndpoint>(*source_url));
  }

  if (!file_system_sources.empty()) {
    base::Pickle pickle;
    ui::WriteCustomDataToPickle(
        std::unordered_map<std::u16string, std::u16string>(
            {{u"fs/sources", file_system_sources}}),
        &pickle);
    data->SetPickledData(ui::ClipboardFormatType::DataTransferCustomType(),
                         pickle);
  }

  if (!filenames.empty()) {
    std::vector<ui::FileInfo> infos;
    for (const auto& filename : filenames) {
      infos.emplace_back(filename, filename.BaseName());
    }
    data->SetFilenames(infos);
  }

  return data;
}

}  // namespace

// Tests -----------------------------------------------------------------------

using DragDropUtilTest = NoSessionAshTestBase;

// Verifies that `drag_drop::ExtractFilePaths()` is WAI.
TEST_F(DragDropUtilTest, ExtractFilePaths) {
  std::unique_ptr<ui::OSExchangeData> data;
  std::u16string file_system_sources;
  std::vector<base::FilePath> filenames;

  // Configure the `TestShellDelegate` to crack file system URLs.
  static_cast<TestShellDelegate*>(Shell::Get()->shell_delegate())
      ->SetCrackFileSystemUrlCallback(
          base::BindRepeating([](const GURL& file_system_url) {
            return base::FilePath(base::StrCat(
                {"//path/to/",
                 std::string(
                     &file_system_url.possibly_invalid_spec().back())}));
          }));

  // Case: Empty.
  data = CreateOSExchangeData(file_system_sources, filenames);
  for (bool fallback_to_filenames : {false, true}) {
    EXPECT_TRUE(ExtractFilePaths(*data, fallback_to_filenames).empty());
  }

  // Case: Only file system sources from trusted Files app source.
  file_system_sources = u"file-system:a\nfile-system:b\nfile-system:c";
  data = CreateOSExchangeData(file_system_sources, filenames,
                              GURL("chrome://file-manager/"));
  for (bool fallback_to_filenames : {false, true}) {
    EXPECT_THAT(
        ExtractFilePaths(*data, fallback_to_filenames),
        testing::ElementsAre(testing::Eq(base::FilePath("//path/to/a")),
                             testing::Eq(base::FilePath("//path/to/b")),
                             testing::Eq(base::FilePath("//path/to/c"))));
  }

  // Case: File system sources with no source endpoint are ignored.
  data = CreateOSExchangeData(file_system_sources, filenames);
  for (bool fallback_to_filenames : {false, true}) {
    EXPECT_TRUE(ExtractFilePaths(*data, fallback_to_filenames).empty());
  }

  // Case: File system sources from untrusted origin are ignored.
  data = CreateOSExchangeData(file_system_sources, filenames,
                              GURL("https://evil.example.com/"));
  for (bool fallback_to_filenames : {false, true}) {
    EXPECT_TRUE(ExtractFilePaths(*data, fallback_to_filenames).empty());
  }

  // Case: Both file system sources (from Files app) and filenames.
  filenames.emplace_back("//path/to/d");
  filenames.emplace_back("//path/to/e");
  filenames.emplace_back("//path/to/f");
  data = CreateOSExchangeData(file_system_sources, filenames,
                              GURL("chrome://file-manager/"));
  for (bool fallback_to_filenames : {false, true}) {
    EXPECT_THAT(
        ExtractFilePaths(*data, fallback_to_filenames),
        testing::ElementsAre(testing::Eq(base::FilePath("//path/to/a")),
                             testing::Eq(base::FilePath("//path/to/b")),
                             testing::Eq(base::FilePath("//path/to/c"))));
  }

  // Case: Both file system sources (with no source endpoint) and filenames.
  data = CreateOSExchangeData(file_system_sources, filenames);
  for (bool fallback_to_filenames : {false, true}) {
    EXPECT_THAT(
        ExtractFilePaths(*data, fallback_to_filenames),
        testing::Conditional(
            fallback_to_filenames,
            testing::ElementsAre(testing::Eq(base::FilePath("//path/to/d")),
                                 testing::Eq(base::FilePath("//path/to/e")),
                                 testing::Eq(base::FilePath("//path/to/f"))),
            testing::IsEmpty()));
  }

  // Case: Only filenames.
  file_system_sources.clear();
  data = CreateOSExchangeData(file_system_sources, filenames);
  for (bool fallback_to_filenames : {false, true}) {
    EXPECT_THAT(
        ExtractFilePaths(*data, fallback_to_filenames),
        testing::Conditional(
            fallback_to_filenames,
            testing::ElementsAre(testing::Eq(base::FilePath("//path/to/d")),
                                 testing::Eq(base::FilePath("//path/to/e")),
                                 testing::Eq(base::FilePath("//path/to/f"))),
            testing::IsEmpty()));
  }
}

}  // namespace ash::drag_drop
