// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/file_suggestion_provider.h"

namespace app_list {

FileSuggestionProvider::FileSuggestionProvider(
    base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback)
    : notify_update_callback_(notify_update_callback) {}

FileSuggestionProvider::~FileSuggestionProvider() = default;

void FileSuggestionProvider::NotifySuggestionUpdate(FileSuggestionType type) {
  notify_update_callback_.Run(type);
}

}  // namespace app_list
