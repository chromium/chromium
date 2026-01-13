// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/views/eula_dialog_linux.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/util/initial_preferences.h"
#include "ui/views/widget/widget.h"

namespace first_run::internal {

namespace {

// This must match kEulaSentinelFile in chrome/installer/util/util_constants.cc
constexpr const char kEulaSentinelFile[] = "EULA Accepted";

}  // namespace

bool IsOrganicFirstRun() {
  // We treat all installs as organic.
  return true;
}

base::FilePath InitialPrefsPath() {
  // The standard location of the initial prefs is next to the chrome binary.
  base::FilePath dir_exe;
  if (!base::PathService::Get(base::DIR_EXE, &dir_exe)) {
    return base::FilePath();
  }

  return installer::InitialPreferences::Path(dir_exe);
}

bool ShowEulaDialog() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    return false;
  }

  base::FilePath sentinel = user_data_dir.Append(kEulaSentinelFile);
  if (base::PathExists(sentinel)) {
    return true;
  }

  base::RunLoop run_loop;
  bool accepted = false;

  auto callback = base::BindOnce(
      [](bool* accepted_ptr, base::RepeatingClosure quit, bool success) {
        *accepted_ptr = success;
        quit.Run();
      },
      &accepted, run_loop.QuitClosure());

  views::Widget* widget = EulaDialog::Show(std::move(callback));
  base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();

  run_loop.Run();

  if (accepted) {
    base::WriteFile(sentinel, "");
  }

  if (weak_widget) {
    weak_widget->CloseNow();
  }

  return accepted;
}

}  // namespace first_run::internal
