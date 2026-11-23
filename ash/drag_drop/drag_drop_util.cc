// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_util.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/pickle.h"
#include "base/strings/string_split.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/gurl.h"

namespace ash::drag_drop {
namespace {

// Helpers ---------------------------------------------------------------------

// Returns the file paths extracted from the specified `data` at the filenames
// storage location.
std::vector<base::FilePath> ExtractFilePathsFromFilenames(
    const ui::OSExchangeData& data) {
  std::vector<base::FilePath> paths;

  if (!data.HasFile()) {
    return paths;
  }

  std::optional<std::vector<ui::FileInfo>> filenames = data.GetFilenames();
  if (!filenames.has_value()) {
    return paths;
  }

  return base::ToVector(filenames.value(), &ui::FileInfo::path);
}

// TODO(http://b/279031685): Ask Files app team to own a util API for this.
// Returns the file paths extracted from the specified `data` at the file
// system sources storage location used by the Files app.
std::vector<base::FilePath> ExtractFilePathsFromFileSystemSources(
    const ui::OSExchangeData& data) {
  std::vector<base::FilePath> paths;

  // Only trust custom "fs/sources" data when the drag originated from the
  // ChromeOS Files app.
  const ui::DataTransferEndpoint* const data_src = data.GetSource();
  if (!data_src || !data_src->IsUrlType() ||
      !data_src->GetURL()->SchemeIs("chrome") ||
      data_src->GetURL()->GetHost() != "file-manager") {
    return paths;
  }

  std::optional<base::Pickle> pickle =
      data.GetPickledData(ui::ClipboardFormatType::DataTransferCustomType());
  if (!pickle.has_value()) {
    return paths;
  }

  std::optional<std::u16string> maybe_sources =
      ui::ReadCustomDataForType(pickle.value(), u"fs/sources");
  if (!maybe_sources.has_value()) {
    return paths;
  }

  const ShellDelegate* const shell_delegate = Shell::Get()->shell_delegate();
  for (const auto& source :
       base::SplitStringPiece(*maybe_sources, u"\n", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (auto path = shell_delegate->CrackFileSystemUrl(GURL(source));
        !path.empty()) {
      paths.emplace_back(std::move(path));
    }
  }

  return paths;
}

}  // namespace

// Utilities -------------------------------------------------------------------

std::vector<base::FilePath> ExtractFilePaths(const ui::OSExchangeData& data,
                                             bool fallback_to_filenames) {
  // Prefer extracting file paths from file system sources when possible. The
  // Files app populates both file system sources and filenames storage
  // locations, but only the former contains directory paths.
  auto paths = ExtractFilePathsFromFileSystemSources(data);
  return (paths.empty() && fallback_to_filenames)
             ? ExtractFilePathsFromFilenames(data)
             : paths;
}

}  // namespace ash::drag_drop
