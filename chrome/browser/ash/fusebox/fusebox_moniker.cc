// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fusebox/fusebox_moniker.h"

#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace fusebox {

// static
MonikerMap::ExtractTokenResult MonikerMap::ExtractToken(
    const std::string& fs_url_as_string) {
  size_t n = 0;
  if (base::StartsWith(fs_url_as_string, fusebox::kMonikerSubdir)) {
    n = strlen(fusebox::kMonikerSubdir);
  } else if (base::StartsWith(fs_url_as_string,
                              fusebox::kMonikerFileSystemURL)) {
    n = strlen(fusebox::kMonikerFileSystemURL);
  } else {
    ExtractTokenResult result;
    result.result_type = ExtractTokenResult::ResultType::NOT_A_MONIKER_FS_URL;
    return result;
  }
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
std::string MonikerMap::GetFilename(const Moniker& moniker) {
  return fusebox::kMonikerFilenamePrefixWithTrailingSlash + moniker.ToString();
}

MonikerMap::MonikerMap() = default;
MonikerMap::~MonikerMap() = default;

Moniker MonikerMap::CreateMoniker(storage::FileSystemURL target) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Moniker moniker = base::Token::CreateRandom();
  map_.insert({moniker, std::move(target)});
  return moniker;
}

void MonikerMap::DestroyMoniker(const Moniker& moniker) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = map_.find(moniker);
  if (iter != map_.end()) {
    map_.erase(iter);
  }
}

storage::FileSystemURL MonikerMap::Resolve(const Moniker& moniker) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = map_.find(moniker);
  if (iter != map_.end()) {
    return iter->second;
  }
  return storage::FileSystemURL();
}

}  // namespace fusebox
