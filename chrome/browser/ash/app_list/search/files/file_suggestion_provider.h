// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_SUGGESTION_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_SUGGESTION_PROVIDER_H_

#include "base/functional/callback.h"
#include "chrome/browser/ash/app_list/search/files/file_suggest_util.h"

namespace app_list {

// The base class of file suggestion providers (such as drive file suggestion
// provider). A subclass should ensure that `NotifySuggestionUpdate()` is
// called when the suggestions managed by the provider update.
class FileSuggestionProvider {
 public:
  explicit FileSuggestionProvider(
      base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback);
  FileSuggestionProvider(const FileSuggestionProvider&) = delete;
  FileSuggestionProvider& operator=(const FileSuggestionProvider&) = delete;
  virtual ~FileSuggestionProvider();

  // Queries for the suggested files managed by this provider and returns the
  // suggested file data, including file paths and suggestion reasons, through
  // the callback. The returned suggestions have been filtered by the file
  // last modification time. Only the files that have been modified more
  // recently than a threshold are returned.
  // NOTE: the overridden function should ensure that `callback` runs finally.
  virtual void GetSuggestFileData(GetSuggestFileDataCallback callback) = 0;

 protected:
  // Sends the notification that the suggestions of `type` update.
  void NotifySuggestionUpdate(FileSuggestionType type);

 private:
  base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_SUGGESTION_PROVIDER_H_
