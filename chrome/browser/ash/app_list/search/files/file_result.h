// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_RESULT_H_

#include <iosfwd>
#include <optional>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

class Profile;

namespace ash {
class ThumbnailLoader;
namespace string_matching {
class TokenizedString;
}  // namespace string_matching
}  // namespace ash

namespace app_list {

// This result class is shared between four file search providers:
// {drive,local} {zero-state,search}.
class FileResult : public ChromeSearchResult, public ash::ColorModeObserver {
 public:
  enum class Type { kFile, kDirectory, kSharedDirectory };

  // Note: `thumbnail_loader_` is only used for list and image search items to
  // load the thumbnail image to be used for the result. It can be nullptr, in
  // which case the result will use the default file type icon for the result.
  FileResult(const std::string& id,
             const base::FilePath& filepath,
             const std::optional<std::u16string>& details,
             ResultType result_type,
             DisplayType display_type,
             float relevance,
             const std::u16string& query,
             Type type,
             Profile* profile,
             ash::ThumbnailLoader* thumbnail_loader);
  ~FileResult() override;

  FileResult(const FileResult&) = delete;
  FileResult& operator=(const FileResult&) = delete;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;
  std::optional<std::string> DriveId() const override;
  std::optional<GURL> url() const override;

  // Calculates file's match relevance score. Will return a default score if the
  // query is missing or the filename is empty.
  static double CalculateRelevance(
      const std::optional<ash::string_matching::TokenizedString>& query,
      const base::FilePath& filepath,
      const std::optional<base::Time>& last_accessed);


  void set_drive_id(const std::optional<std::string>& drive_id) {
    drive_id_ = drive_id;
  }

  void set_url(const std::optional<GURL>& url) { url_ = url; }

 private:
  // ash::ColorModeObserver:
  void OnColorModeChanged(bool dark_mode_enabled) override;

  // Called by `thumbnail_image_` to generate the thumbnail image to be used for
  // the result icon.
  void RequestThumbnail(const base::FilePath& file_path,
                        const gfx::Size& size,
                        ash::HoldingSpaceImage::BitmapCallback callback);

  // Called by `thumbnail_image_` to generate the placeholder image to be used
  // for the result icon.
  gfx::ImageSkia GetPlaceholderImage(const base::FilePath& file_path,
                                     const gfx::Size& size,
                                     const std::optional<bool>& dark_background,
                                     const std::optional<bool>& is_folder);

  void UpdateChipIcon();
  void UpdateThumbnailIcon();

  const base::FilePath filepath_;
  const Type type_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<ash::ThumbnailLoader> thumbnail_loader_;

  std::optional<std::string> drive_id_;
  std::optional<GURL> url_;

  // Helper to handle lazy loading of the file thumbnail image - it manages an
  // ImageSkia that defaults to a "placeholder" icon - the default file icon,
  // and request a thumbnail load, using `thumbnail_loader_` when the image is
  // first requested (e.g. to paint it in the UI).
  // Allows for `thumbnail_loader_` to be nullptr, in which case the image will
  // remain in placeholder state.
  std::unique_ptr<ash::HoldingSpaceImage> thumbnail_image_;
  std::optional<base::CallbackListSubscription> thumbnail_image_update_sub_;

  base::WeakPtrFactory<FileResult> weak_factory_{this};
};

::std::ostream& operator<<(::std::ostream& os, const FileResult& result);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_RESULT_H_
