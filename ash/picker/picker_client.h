// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_CLIENT_H_
#define ASH_PICKER_PICKER_CLIENT_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/picker_web_paste_target.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "url/gurl.h"

class SkBitmap;
class PrefService;

namespace gfx {
class Size;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

// Lets PickerController in Ash to communicate with the browser.
class ASH_EXPORT PickerClient {
 public:
  using CrosSearchResultsCallback =
      base::RepeatingCallback<void(ash::AppListSearchResultType result_type,
                                   std::vector<PickerSearchResult> results)>;
  using ShowEditorCallback =
      base::OnceCallback<void(std::optional<std::string> preset_query_id,
                              std::optional<std::string> freeform_text)>;
  using ShowLobsterCallback =
      base::OnceCallback<void(std::optional<std::string> query)>;
  using SuggestedEditorResultsCallback =
      base::OnceCallback<void(std::vector<PickerSearchResult>)>;
  using RecentFilesCallback =
      base::OnceCallback<void(std::vector<PickerSearchResult>)>;
  using SuggestedLinksCallback =
      base::RepeatingCallback<void(std::vector<PickerSearchResult>)>;
  using FetchFileThumbnailCallback =
      base::OnceCallback<void(const SkBitmap* bitmap, base::File::Error error)>;

  // Gets the SharedURLLoaderFactory to use for Picker network requests, e.g. to
  // fetch assets. This is the loader factory for the active profile, not the
  // global browser process one.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetSharedURLLoaderFactory() = 0;

  // Starts a search using the CrOS Search API
  // (`app_list::SearchEngine::StartSearch`).
  virtual void StartCrosSearch(const std::u16string& query,
                               std::optional<PickerCategory> category,
                               CrosSearchResultsCallback callback) = 0;
  // Stops a search using the CrOS Search API
  // (`app_list::SearchEngine::StopQuery`).
  virtual void StopCrosQuery() = 0;

  // Whether this device is eligble for editor.
  virtual bool IsEligibleForEditor() = 0;

  // Caches the current input field context and returns a callback to show
  // Editor. If Editor is not available, this returns a null callback.
  virtual ShowEditorCallback CacheEditorContext() = 0;

  virtual ShowLobsterCallback GetShowLobsterCallback() = 0;

  virtual void GetSuggestedEditorResults(
      SuggestedEditorResultsCallback callback) = 0;

  virtual void GetRecentLocalFileResults(size_t max_files,
                                         base::TimeDelta now_delta,
                                         RecentFilesCallback callback) = 0;

  virtual void GetRecentDriveFileResults(size_t max_files,
                                         RecentFilesCallback callback) = 0;

  virtual void GetSuggestedLinkResults(size_t max_results,
                                       SuggestedLinksCallback callback) = 0;

  virtual void FetchFileThumbnail(const base::FilePath& path,
                                  const gfx::Size& size,
                                  FetchFileThumbnailCallback callback) = 0;

  virtual PrefService* GetPrefs() = 0;
  // SAFETY: The returned `do_paste` MUST be called synchronously. Calling it
  // after a delay, such as in a different task, may result in use-after-frees.
  virtual std::optional<PickerWebPasteTarget> GetWebPasteTarget() = 0;

  // Make an announcement via an offscreen live region.
  virtual void Announce(std::u16string_view message) = 0;

 protected:
  PickerClient();
  virtual ~PickerClient();
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_CLIENT_H_
