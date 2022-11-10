// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fusebox/fusebox_moniker.h"

#include "base/strings/strcat.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace fusebox {

// static
MonikerMap::ExtractTokenResult MonikerMap::ExtractToken(
    const std::string& fs_url_as_string) {
  if (!base::StartsWith(fs_url_as_string, fusebox::kMonikerSubdir)) {
    ExtractTokenResult result;
    result.result_type = ExtractTokenResult::ResultType::NOT_A_MONIKER_FS_URL;
    return result;
  }
  size_t n = strlen(fusebox::kMonikerSubdir);
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

Moniker MonikerMap::CreateMoniker(const storage::FileSystemURL& target,
                                  bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Moniker moniker = base::Token::CreateRandom();
  map_.insert({moniker, std::make_pair(target, read_only)});
  return moniker;
}

void MonikerMap::DestroyMoniker(const Moniker& moniker) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = map_.find(moniker);
  if (iter != map_.end()) {
    map_.erase(iter);
  }
}

MonikerMap::FSURLAndReadOnlyState MonikerMap::Resolve(
    const Moniker& moniker) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = map_.find(moniker);
  if (iter != map_.end()) {
    return iter->second;
  }
  return std::make_pair(storage::FileSystemURL(), false);
}

base::Value MonikerMap::GetDebugJSON() {
  base::Value::Dict dict;
  for (const auto& i : map_) {
    dict.Set(i.first.ToString(),
             base::Value(base::StrCat(
                 {i.second.first.ToGURL().spec(),
                  i.second.second ? " (read-only)" : " (read-write)"})));
  }
  return base::Value(std::move(dict));
}

}  // namespace fusebox
