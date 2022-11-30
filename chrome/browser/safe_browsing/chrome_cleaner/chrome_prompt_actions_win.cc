// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_actions_win.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_scanner_results_win.h"

namespace safe_browsing {

ChromePromptActions::ChromePromptActions(PromptUserCallback on_prompt_user)
    : on_prompt_user_(std::move(on_prompt_user)) {
  DCHECK(on_prompt_user_);
}

ChromePromptActions::~ChromePromptActions() {}

void ChromePromptActions::PromptUser(
    const std::vector<base::FilePath>& files_to_delete,
    const absl::optional<std::vector<std::wstring>>& registry_keys,
    PromptUserReplyCallback callback) {
  using FileCollection = ChromeCleanerScannerResults::FileCollection;
  using RegistryKeyCollection =
      ChromeCleanerScannerResults::RegistryKeyCollection;

  DCHECK(on_prompt_user_);
  ChromeCleanerScannerResults scanner_results(
      FileCollection(files_to_delete.begin(), files_to_delete.end()),
      registry_keys
          ? RegistryKeyCollection(registry_keys->begin(), registry_keys->end())
          : RegistryKeyCollection());
  std::move(on_prompt_user_)
      .Run(std::move(scanner_results), std::move(callback));
}

}  // namespace safe_browsing
