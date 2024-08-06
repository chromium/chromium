// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UNTRUSTED_UI_H_
#define ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UNTRUSTED_UI_H_

#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {
namespace file_manager {

class FileManagerUntrustedUI;

// Class that stores properties for the chrome-untrusted://file-manager WebUI.
class FileManagerUntrustedUIConfig
    : public content::DefaultWebUIConfig<FileManagerUntrustedUI> {
 public:
  FileManagerUntrustedUIConfig();
  ~FileManagerUntrustedUIConfig() override;
};

// WebUI for chrome-untrusted://file-manager, intended to be used by the file
// manager when untrusted content needs to be processed.
class FileManagerUntrustedUI : public ui::UntrustedWebUIController {
 public:
  explicit FileManagerUntrustedUI(content::WebUI* web_ui);
  FileManagerUntrustedUI(const FileManagerUntrustedUI&) = delete;
  FileManagerUntrustedUI& operator=(const FileManagerUntrustedUI&) = delete;
  ~FileManagerUntrustedUI() override;
};

}  // namespace file_manager
}  // namespace ash

#endif  // ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UNTRUSTED_UI_H_
