// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGESTION_PROVIDER_H_
#define CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGESTION_PROVIDER_H_

#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"

namespace ash {

class FileSuggestKeyedService;

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

  // Requests to update the data in `item_suggest_cache_`. Only used by the file
  // suggest keyed service.
  // This can be removed when kContinueSectionWithRecents feature gets enabled.
  // TODO(https://crbug.com/1356347): Now the app list relies on this service to
  // fetch the drive suggestion data. Meanwhile, this service relies on the app
  // list to trigger the item cache update. This cyclic dependency could be
  // confusing. The service should update the data cache by its own without
  // depending on the app list code.
  virtual void MaybeUpdateItemSuggestCache(
      base::PassKey<FileSuggestKeyedService>) = 0;

 protected:
  // Sends the notification that the suggestions of `type` update.
  void NotifySuggestionUpdate(FileSuggestionType type);

 private:
  base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGESTION_PROVIDER_H_
