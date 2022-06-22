// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/fusebox_moniker.h"

#include <unordered_map>
#include "base/no_destructor.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

base::NoDestructor<
    std::unordered_map<base::Token, storage::FileSystemURL, base::TokenHash>>
    fusebox_monikers;

}  // namespace

namespace file_manager {

// static
FuseBoxMoniker FuseBoxMoniker::Create(storage::FileSystemURL target) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Token token = base::Token::CreateRandom();
  fusebox_monikers->insert({token, std::move(target)});
  return FuseBoxMoniker(token);
}

// static
FuseBoxMoniker::ExtractTokenResult FuseBoxMoniker::ExtractToken(
    std::string fs_url_as_string) {
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
storage::FileSystemURL FuseBoxMoniker::Resolve(base::Token token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = fusebox_monikers->find(token);
  if (iter != fusebox_monikers->end()) {
    return iter->second;
  }
  return storage::FileSystemURL();
}

FuseBoxMoniker::FuseBoxMoniker(base::Token token) : token_(token) {}

void FuseBoxMoniker::Destroy() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = fusebox_monikers->find(token_);
  if (iter != fusebox_monikers->end()) {
    fusebox_monikers->erase(iter);
  }
}

storage::FileSystemURL FuseBoxMoniker::Target() {
  return Resolve(token_);
}

std::string FuseBoxMoniker::LinkFilename() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return fusebox::kMonikerFilenamePrefixWithTrailingSlash + token_.ToString();
}

base::Token FuseBoxMoniker::LinkToken() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return token_;
}

}  // namespace file_manager
