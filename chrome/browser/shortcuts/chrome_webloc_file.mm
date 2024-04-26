// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/chrome_webloc_file.h"

#import <Foundation/Foundation.h>

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/files/block_tests_writing_to_special_dirs.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"

namespace shortcuts {

namespace {

NSString* const kUrlKey = @"URL";
NSString* const kProfileKey = @"CrProfile";

}  // namespace

ChromeWeblocFile::ChromeWeblocFile(GURL target_url,
                                   base::SafeBaseName profile_path_name)
    : target_url_(std::move(target_url)),
      profile_path_name_(std::move(profile_path_name)) {}

ChromeWeblocFile::~ChromeWeblocFile() = default;

// static
std::optional<ChromeWeblocFile> ChromeWeblocFile::LoadFromFile(
    const base::FilePath& file_path) {
  NSDictionary* contents;
  NSError* error;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    contents = [NSDictionary
        dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(file_path)
                              error:&error];
  }
  if (!contents) {
    LOG(ERROR) << "Failed to read shortcut file from " << file_path << ": "
               << error;
    return std::nullopt;
  }
  GURL target_url(base::SysNSStringToUTF8(contents[kUrlKey]));
  std::optional<base::SafeBaseName> profile_path_name =
      base::SafeBaseName::Create(
          base::apple::NSStringToFilePath(contents[kProfileKey]));
  if (!target_url.is_valid() || !profile_path_name.has_value() ||
      profile_path_name->path().empty()) {
    LOG(ERROR) << "Failed to parse URL or profile from " << file_path << " "
               << contents;
    return std::nullopt;
  }
  return ChromeWeblocFile(std::move(target_url), *std::move(profile_path_name));
}

bool ChromeWeblocFile::SaveToFile(const base::FilePath& file_path) {
  if (!base::BlockTestsWritingToSpecialDirs::CanWriteToPath(file_path)) {
    return false;
  }

  NSDictionary* contents = @{
    kUrlKey : base::SysUTF8ToNSString(target_url_.spec()),
    kProfileKey : base::apple::FilePathToNSString(profile_path_name_.path()),
  };
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    NSError* error = nil;
    if (![contents writeToURL:base::apple::FilePathToNSURL(file_path)
                        error:&error]) {
      LOG(ERROR) << "Failed to create shortcut at " << file_path << ": "
                 << error;
      return false;
    }
  }
  return true;
}

}  // namespace shortcuts
