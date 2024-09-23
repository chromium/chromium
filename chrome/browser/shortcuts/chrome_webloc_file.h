// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_CHROME_WEBLOC_FILE_H_
#define CHROME_BROWSER_SHORTCUTS_CHROME_WEBLOC_FILE_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/files/safe_base_name.h"
#include "url/gurl.h"

namespace shortcuts {

// This class can be used to save and load .crwebloc files, a variant of regular
// .webloc files that also includes profile information when opened by Chrome.
// There's not really any reason these files follow the .webloc file format,
// other than that it is a relatively easy file format to deal with. The fact
// that these files extend the .webloc file format does not give them any of
// the OS behavior that .webloc files have.
//
// Note that this file does not store the specific Chrome binary/channel that
// created a shortcut. If shortcuts needs to be associated with a specific
// Chrome channel or binary, the created file needs to be explicitly associated
// with the current Chrome binary using the available macOS APIs.
//
// This file format is only used (and supported) on macOS.
class ChromeWeblocFile {
 public:
  static constexpr base::FilePath::StringPieceType kFileExtension = ".crwebloc";

  ChromeWeblocFile(GURL target_url, base::SafeBaseName profile_path_name);
  ~ChromeWeblocFile();

  static std::optional<ChromeWeblocFile> LoadFromFile(
      const base::FilePath& file_path);
  bool SaveToFile(const base::FilePath& file_path);

  const GURL& target_url() const { return target_url_; }
  const base::SafeBaseName& profile_path_name() const {
    return profile_path_name_;
  }

 private:
  GURL target_url_;
  base::SafeBaseName profile_path_name_;
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_CHROME_WEBLOC_FILE_H_
