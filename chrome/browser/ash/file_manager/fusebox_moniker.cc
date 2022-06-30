// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/fusebox_moniker.h"

#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace file_manager {

// static
FuseBoxMonikerMap::ExtractTokenResult FuseBoxMonikerMap::ExtractToken(
    const std::string& fs_url_as_string) {
  if (!base::StartsWith(fs_url_as_string, fusebox::kMonikerFileSystemURL)) {
    ExtractTokenResult result;
    result.result_type = ExtractTokenResult::ResultType::NOT_A_MONIKER_FS_URL;
    return result;
  }
  const size_t n = strlen(fusebox::kMonikerFileSystemURL);
  if (fs_url_as_string.size() <= n) {
    ExtractTokenResult result;
    result.result_type =
        ExtractTokenResult::ResultType::MONIKER_FS_URL_BUT_ONLY_ROOT;
    return result;
  } else if (fs_url_as_string[n] != '/') {
    ExtractTokenResult result;
    result.result_type = ExtractTokenResult::ResultType::NOT_A_MONIKER_FS_URL;
    return result;
  }
  absl::optional<base::Token> token =
      base::Token::FromString(fs_url_as_string.substr(n + 1));
  if (!token) {
    ExtractTokenResult result;
    result.result_type =
        ExtractTokenResult::ResultType::MONIKER_FS_URL_BUT_NOT_WELL_FORMED;
    return result;
  }
  ExtractTokenResult result;
  result.result_type = ExtractTokenResult::ResultType::OK;
  result.token = *token;
  return result;
}

// static
std::string FuseBoxMonikerMap::GetFilename(const FuseBoxMoniker& moniker) {
  return fusebox::kMonikerFilenamePrefixWithTrailingSlash + moniker.ToString();
}

FuseBoxMonikerMap::FuseBoxMonikerMap() = default;
FuseBoxMonikerMap::~FuseBoxMonikerMap() = default;

FuseBoxMoniker FuseBoxMonikerMap::CreateMoniker(storage::FileSystemURL target) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  FuseBoxMoniker moniker = base::Token::CreateRandom();
  map_.insert({moniker, std::move(target)});
  return moniker;
}

void FuseBoxMonikerMap::DestroyMoniker(const FuseBoxMoniker& moniker) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = map_.find(moniker);
  if (iter != map_.end()) {
    map_.erase(iter);
  }
}

storage::FileSystemURL FuseBoxMonikerMap::Resolve(
    const FuseBoxMoniker& moniker) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = map_.find(moniker);
  if (iter != map_.end()) {
    return iter->second;
  }
  return storage::FileSystemURL();
}

}  // namespace file_manager
