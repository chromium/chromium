// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_CHROME_SCANNING_APP_DELEGATE_H_
#define CHROME_BROWSER_ASH_SCANNING_CHROME_SCANNING_APP_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/webui/scanning/scanning_app_delegate.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/scanning/scanning_file_path_helper.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class WebUI;
}  // namespace content

namespace ui {
class SelectFilePolicy;
}  // namespace ui

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {

class ChromeScanningAppDelegate : public ScanningAppDelegate {
 public:
  explicit ChromeScanningAppDelegate(content::WebUI* web_ui);
  ~ChromeScanningAppDelegate() override;

  ChromeScanningAppDelegate(const ChromeScanningAppDelegate&) = delete;
  ChromeScanningAppDelegate& operator=(const ChromeScanningAppDelegate&) =
      delete;

  // ScanningAppDelegate:
  std::unique_ptr<ui::SelectFilePolicy> CreateChromeSelectFilePolicy() override;
  std::string GetBaseNameFromPath(const base::FilePath& path) override;
  base::FilePath GetMyFilesPath() override;
  std::string GetScanSettingsFromPrefs() override;
  bool IsFilePathSupported(const base::FilePath& path_to_file) override;
  void OpenFilesInMediaApp(
      const std::vector<base::FilePath>& file_paths) override;
  void SaveScanSettingsToPrefs(const std::string& scan_settings) override;
  void ShowFileInFilesApp(const base::FilePath& path_to_file,
                          base::OnceCallback<void(bool)> callback) override;
  BindScanServiceCallback GetBindScanServiceCallback(
      content::WebUI* web_ui) override;

  // Register scan settings prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Initializes ScanningFilePathHelper with |google_drive_path_| and
  // |my_files_path_|.
  void SetValidPaths(const base::FilePath& google_drive_path,
                     const base::FilePath& my_files_path);

  void SetRemoveableMediaPathForTesting(const base::FilePath& path);

 private:
  // Returns the PrefService for the active Profile.
  PrefService* GetPrefs() const;

  // Callback for ShowFileInFilesApp().
  void OnPathExists(const base::FilePath& path_to_file,
                    base::OnceCallback<void(bool)>,
                    bool file_path_exists);

  raw_ptr<content::WebUI, DanglingUntriaged> web_ui_;  // Owns |this|.

  // Helper class for for file path manipulation and verification.
  ScanningFilePathHelper file_path_helper_;

  // Task runner for the I/O function base::PathExists().
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<ChromeScanningAppDelegate> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_CHROME_SCANNING_APP_DELEGATE_H_
