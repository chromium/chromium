// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PICKER_PICKER_CLIENT_H_
#define ASH_PUBLIC_CPP_PICKER_PICKER_CLIENT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "url/gurl.h"

class SkBitmap;
class PrefService;

namespace gfx {
class Size;
}

namespace ash {

// Lets PickerController in Ash to communicate with the browser.
class ASH_PUBLIC_EXPORT PickerClient {
 public:
  using CrosSearchResultsCallback =
      base::RepeatingCallback<void(ash::AppListSearchResultType result_type,
                                   std::vector<PickerSearchResult> results)>;
  using ShowEditorCallback =
      base::OnceCallback<void(std::optional<std::string> preset_query_id,
                              std::optional<std::string> freeform_text)>;
  using SuggestedEditorResultsCallback =
      base::OnceCallback<void(std::vector<PickerSearchResult>)>;
  using RecentFilesCallback =
      base::OnceCallback<void(std::vector<PickerSearchResult>)>;
  using SuggestedLinksCallback =
      base::RepeatingCallback<void(std::vector<PickerSearchResult>)>;
  using FetchFileThumbnailCallback =
      base::OnceCallback<void(const SkBitmap* bitmap, base::File::Error error)>;

  // Starts a search using the CrOS Search API
  // (`app_list::SearchEngine::StartSearch`).
  virtual void StartCrosSearch(const std::u16string& query,
                               std::optional<PickerCategory> category,
                               CrosSearchResultsCallback callback) = 0;
  // Stops a search using the CrOS Search API
  // (`app_list::SearchEngine::StopQuery`).
  virtual void StopCrosQuery() = 0;

  // Caches the current input field context and returns a callback to show
  // Editor. If Editor is not available, this returns a null callback.
  virtual ShowEditorCallback CacheEditorContext() = 0;

  virtual void GetSuggestedEditorResults(
      SuggestedEditorResultsCallback callback) = 0;

  virtual void GetRecentLocalFileResults(size_t max_files,
                                         RecentFilesCallback callback) = 0;

  virtual void GetRecentDriveFileResults(size_t max_files,
                                         RecentFilesCallback callback) = 0;

  virtual void GetSuggestedLinkResults(SuggestedLinksCallback callback) = 0;

  virtual bool IsFeatureAllowedForDogfood() = 0;

  virtual void FetchFileThumbnail(const base::FilePath& path,
                                  const gfx::Size& size,
                                  FetchFileThumbnailCallback callback) = 0;

  virtual PrefService* GetPrefs() = 0;

 protected:
  PickerClient();
  virtual ~PickerClient();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PICKER_PICKER_CLIENT_H_
