// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SPARKY_SPARKY_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_SPARKY_SPARKY_DELEGATE_IMPL_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/sparky/storage/simple_size_calculator.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chromeos/ash/components/sparky/snapshot_util.h"
#include "components/manta/proto/sparky.pb.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/manta/sparky/system_info_delegate.h"

class Profile;

namespace ash {

class SparkyDelegateImpl : public manta::SparkyDelegate,
                           SimpleSizeCalculator::Observer {
 public:
  explicit SparkyDelegateImpl(Profile* profile);
  ~SparkyDelegateImpl() override;

  SparkyDelegateImpl(const SparkyDelegateImpl&) = delete;
  SparkyDelegateImpl& operator=(const SparkyDelegateImpl&) = delete;

  // manta::SparkyDelegate
  bool SetSettings(std::unique_ptr<manta::SettingsData> settings_data) override;
  SettingsDataList* GetSettingsList() override;
  std::optional<base::Value> GetSettingValue(
      const std::string& setting_id) override;
  void GetScreenshot(manta::ScreenshotDataCallback callback) override;
  std::vector<manta::AppsData> GetAppsList() override;
  void LaunchApp(const std::string& app_id) override;
  void ObtainStorageInfo(manta::StorageDataCallback storage_callback) override;
  void Click(int x, int y) override;
  void KeyboardEntry(std::string text) override;
  void KeyPress(const std::string& key,
                bool control,
                bool alt,
                bool shift) override;
  void GetMyFiles(manta::FilesDataCallback callback,
                  bool obtain_bytes,
                  std::set<std::string> allowed_file_paths) override;
  void LaunchFile(const std::string& file_path) override;
  void WriteFile(const std::string& name, const std::string& bytes) override;
  void UpdateFileSummaries(
      const std::vector<manta::FileData>& files_with_summary) override;
  std::vector<manta::FileData> GetFileSummaries() override;

  // SizeCalculator::Observer:
  void OnSizeCalculated(
      const SimpleSizeCalculator::CalculationType& calculation_type,
      int64_t total_bytes) override;

 private:
  friend class SparkyDelegateImplTest;

  void SetRootPathForTesting(const base::FilePath& root_path) {
    root_path_ = root_path;
  }

  void AddPrefToMap(
      const std::string& pref_name,
      extensions::api::settings_private::PrefType settings_pref_type,
      std::optional<base::Value> value);

  void StartObservingCalculators();
  void OnStorageInfoUpdated();
  void StopObservingCalculators();

  const raw_ptr<Profile> profile_;
  std::unique_ptr<extensions::PrefsUtil> prefs_util_;
  SettingsDataList current_prefs_;
  std::unique_ptr<sparky::ScreenshotHandler> screenshot_handler_;
  manta::StorageDataCallback storage_callback_;

  // Instances calculating the size of each storage items.
  TotalDiskSpaceCalculator total_disk_space_calculator_;
  FreeDiskSpaceCalculator free_disk_space_calculator_;

  // Keeps track of the size of each storage item.
  int64_t
      storage_items_total_bytes_[SimpleSizeCalculator::kCalculationTypeCount] =
          {0};

  // Controls if the size of each storage item has been calculated.
  std::bitset<SimpleSizeCalculator::kCalculationTypeCount> calculation_state_;

  // Root path which files will be obtained from.
  base::FilePath root_path_;
  std::vector<base::FilePath> trash_paths_;

  // First value is the file path. The second value contains information on the
  // file include the file summary.
  std::map<std::string, manta::FileData> file_summaries_;

  base::WeakPtrFactory<SparkyDelegateImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SPARKY_SPARKY_DELEGATE_IMPL_H_
