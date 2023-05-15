// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNING_SCANNING_APP_DELEGATE_H_
#define ASH_WEBUI_SCANNING_SCANNING_APP_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/webui/scanning/mojom/scanning.mojom-forward.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ui {
class SelectFilePolicy;
}  // namespace ui

namespace content {
class WebUI;
}  // namespace content

namespace ash {

// A delegate which exposes browser functionality from //chrome to the Scan app
// UI.
class ScanningAppDelegate {
 public:
  using BindScanServiceCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<scanning::mojom::ScanService>)>;

  virtual ~ScanningAppDelegate() = default;

  // Returns a ChromeSelectFilePolicy used to open a select dialog.
  virtual std::unique_ptr<ui::SelectFilePolicy>
  CreateChromeSelectFilePolicy() = 0;

  // Gets the display name from |path| to show in the Scan To dropdown. Handles
  // the special case of converting the Google Drive root and MyFiles directory
  // to the desired display names "Google Drive" and "My Files" respectively.
  virtual std::string GetBaseNameFromPath(const base::FilePath& path) = 0;

  // Gets the MyFiles path for the current user.
  virtual base::FilePath GetMyFilesPath() = 0;

  // Gets scan settings from Pref service.
  virtual std::string GetScanSettingsFromPrefs() = 0;

  // Determines if |path_to_file| is a supported file path for the Files app.
  virtual bool IsFilePathSupported(const base::FilePath& path_to_file) = 0;

  // Opens the Media app with the files specified in |file_paths|.
  virtual void OpenFilesInMediaApp(
      const std::vector<base::FilePath>& file_paths) = 0;

  // Saves scan settings to Pref service.
  virtual void SaveScanSettingsToPrefs(const std::string& scan_settings) = 0;

  // Opens the Files app with |path_to_file| highlighted. Returns false if
  // |path_to_file| is not found in the filesystem.
  virtual void ShowFileInFilesApp(
      const base::FilePath& path_to_file,
      base::OnceCallback<void(const bool)> callback) = 0;

  virtual BindScanServiceCallback GetBindScanServiceCallback(
      content::WebUI* web_ui) = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_SCANNING_SCANNING_APP_DELEGATE_H_
