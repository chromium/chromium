// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_MODEL_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/fileapi/recent_file.h"
#include "chrome/browser/chromeos/fileapi/recent_source.h"
#include "components/keyed_service/core/keyed_service.h"
#include "storage/browser/file_system/file_system_url.h"

class GURL;
class Profile;

namespace storage {

class FileSystemContext;

}  // namespace storage

namespace chromeos {

class RecentModelFactory;

// Provides a list of recently modified files.
//
// All member functions must be called on the UI thread.
class RecentModel : public KeyedService {
 public:
  using GetRecentFilesCallback =
      base::OnceCallback<void(const std::vector<RecentFile>& files)>;

  ~RecentModel() override;

  // Returns an instance for the given profile.
  static RecentModel* GetForProfile(Profile* profile);

  // Creates an instance with given sources. Only for testing.
  static std::unique_ptr<RecentModel> CreateForTest(
      std::vector<std::unique_ptr<RecentSource>> sources);

  // Returns a list of recent files by querying sources.
  // Files are sorted by descending order of last modified time.
  // Results might be internally cached for better performance.
  void GetRecentFiles(storage::FileSystemContext* file_system_context,
                      const GURL& origin,
                      GetRecentFilesCallback callback);

  // KeyedService overrides:
  void Shutdown() override;

 private:
  friend class RecentModelFactory;
  friend class RecentModelTest;
  FRIEND_TEST_ALL_PREFIXES(RecentModelTest, GetRecentFiles_UmaStats);

  static const char kLoadHistogramName[];

  explicit RecentModel(Profile* profile);
  explicit RecentModel(std::vector<std::unique_ptr<RecentSource>> sources);

  void OnGetRecentFiles(size_t max_files,
                        const base::Time& cutoff_time,
                        std::vector<RecentFile> files);
  void OnGetRecentFilesCompleted();
  void ClearCache();

  void SetMaxFilesForTest(size_t max_files);
  void SetForcedCutoffTimeForTest(const base::Time& forced_cutoff_time);

  std::vector<std::unique_ptr<RecentSource>> sources_;

  // The maximum number of files in Recent. This value won't be changed from
  // default except for unit tests.
  size_t max_files_ = 1000;

  // If this is set to non-null, it is used as a cut-off time. Should be used
  // only in unit tests.
  base::Optional<base::Time> forced_cutoff_time_;

  // Cached GetRecentFiles() response.
  base::Optional<std::vector<RecentFile>> cached_files_ = base::nullopt;

  // Timer to clear the cache.
  base::OneShotTimer cache_clear_timer_;

  // Time when the build started.
  base::TimeTicks build_start_time_;

  // While a recent file list is built, this vector contains callbacks to be
  // invoked with the new list.
  std::vector<GetRecentFilesCallback> pending_callbacks_;

  // Number of in-flight sources building recent file lists.
  int num_inflight_sources_ = 0;

  // Intermediate container of recent files while building a list.
  std::priority_queue<RecentFile, std::vector<RecentFile>, RecentFileComparator>
      intermediate_files_;

  base::WeakPtrFactory<RecentModel> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RecentModel);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_MODEL_H_
