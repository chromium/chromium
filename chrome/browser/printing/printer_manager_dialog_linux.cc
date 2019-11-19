// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_manager_dialog.h"

#include <memory>

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"

namespace {

// Older KDE shipped with system-config-printer-kde, which was buggy. Thus do
// not bother with system-config-printer-kde and just always use
// system-config-printer.
// https://bugs.kde.org/show_bug.cgi?id=271957.
constexpr const char* kSystemConfigPrinterCommand[] = {"system-config-printer",
                                                       nullptr};

// Newer KDE has an improved print manager.
constexpr const char* kKde4KcmPrinterCommand[] = {
    "kcmshell4", "kcm_printer_manager", nullptr};
constexpr const char* kKde5KcmPrinterCommand[] = {
    "kcmshell5", "kcm_printer_manager", nullptr};

// Older GNOME printer manager. Used as a fallback.
constexpr const char* kGnomeControlCenterPrintersCommand[] = {
    "gnome-control-center", "printers", nullptr};

// Returns true if the dialog was opened successfully.
bool OpenPrinterConfigDialog(const char* const* command) {
  DCHECK(command);
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!base::ExecutableExistsInPath(env.get(), *command))
    return false;
  std::vector<std::string> argv;
  while (*command)
    argv.push_back(*command++);
  base::Process process = base::LaunchProcess(argv, base::LaunchOptions());
  if (!process.IsValid())
    return false;
  base::EnsureProcessGetsReaped(std::move(process));
  return true;
}

// Detect the command based on the deskop environment and open the printer
// manager dialog.
void DetectAndOpenPrinterConfigDialog() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::unique_ptr<base::Environment> env(base::Environment::Create());

  bool opened = false;
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      opened = OpenPrinterConfigDialog(kKde4KcmPrinterCommand) ||
               OpenPrinterConfigDialog(kSystemConfigPrinterCommand);
      break;
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
      opened = OpenPrinterConfigDialog(kKde5KcmPrinterCommand) ||
               OpenPrinterConfigDialog(kSystemConfigPrinterCommand);
      break;
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_PANTHEON:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
      opened = OpenPrinterConfigDialog(kSystemConfigPrinterCommand);
      break;
    case base::nix::DESKTOP_ENVIRONMENT_CINNAMON:
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      opened = OpenPrinterConfigDialog(kSystemConfigPrinterCommand) ||
               OpenPrinterConfigDialog(kGnomeControlCenterPrintersCommand);
      break;
  }
  LOG_IF(ERROR, !opened) << "Failed to open printer manager dialog ";
}

}  // namespace

namespace printing {

void PrinterManagerDialog::ShowPrinterManagerDialog(Profile* profile) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&DetectAndOpenPrinterConfigDialog));
}

}  // namespace printing
