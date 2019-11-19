// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/client/shell_main_delegate.h"

#include "ash/shell/content/client/shell_content_browser_client.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "content/public/utility/utility_thread.h"
#include "services/service_manager/public/cpp/service.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace shell {

ShellMainDelegate::ShellMainDelegate() = default;

ShellMainDelegate::~ShellMainDelegate() = default;

bool ShellMainDelegate::BasicStartupComplete(int* exit_code) {
  content::SetContentClient(&content_client_);
  return false;
}

void ShellMainDelegate::PreSandboxStartup() {
  InitializeResourceBundle();
  ui::InitializeInputMethodForTesting();
}

content::ContentBrowserClient* ShellMainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(new ShellContentBrowserClient);
  return browser_client_.get();
}

void ShellMainDelegate::InitializeResourceBundle() {
  // Load ash resources and strings; not 'common' (Chrome) resources.
  base::FilePath path;
  base::PathService::Get(base::DIR_MODULE, &path);
  base::FilePath ash_test_strings =
      path.Append(FILE_PATH_LITERAL("ash_test_strings.pak"));

  ui::ResourceBundle::InitSharedInstanceWithPakPath(ash_test_strings);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_100P)) {
    base::FilePath ash_test_resources_100 = path.Append(
        FILE_PATH_LITERAL("ash_test_resources_with_content_100_percent.pak"));
    rb.AddDataPackFromPath(ash_test_resources_100, ui::SCALE_FACTOR_100P);
  }
  if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_200P)) {
    base::FilePath ash_test_resources_200 =
        path.Append(FILE_PATH_LITERAL("ash_test_resources_200_percent.pak"));
    rb.AddDataPackFromPath(ash_test_resources_200, ui::SCALE_FACTOR_200P);
  }
}

}  // namespace shell
}  // namespace ash
