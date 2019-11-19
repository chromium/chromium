// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_DRIVE_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_DRIVE_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/fileapi/recent_source.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace chromeos {

class RecentFile;

// RecentSource implementation for Drive files.
//
// All member functions must be called on the UI thread.
//
// TODO(nya): Write unit tests.
class RecentDriveSource : public RecentSource {
 public:
  explicit RecentDriveSource(Profile* profile);
  ~RecentDriveSource() override;

  // RecentSource overrides:
  void GetRecentFiles(Params params) override;

 private:
  static const char kLoadHistogramName[];

  void OnComplete();

  void GotSearchResults(
      drive::FileError error,
      base::Optional<std::vector<drivefs::mojom::QueryItemPtr>> results);

  Profile* const profile_;

  // Set at the beginning of GetRecentFiles().
  base::Optional<Params> params_;

  base::TimeTicks build_start_time_;

  std::vector<RecentFile> files_;

  mojo::Remote<drivefs::mojom::SearchQuery> search_query_;

  base::WeakPtrFactory<RecentDriveSource> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RecentDriveSource);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_DRIVE_SOURCE_H_
