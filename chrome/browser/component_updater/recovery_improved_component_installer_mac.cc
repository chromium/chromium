// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/recovery_improved_component_installer.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/update_client.h"
#include "components/version_info/version_info.h"

namespace component_updater {
namespace {

constexpr base::FilePath::CharType kRecoveryExecutableName[] =
    FILE_PATH_LITERAL("ChromeRecovery");

class RecoveryComponentActionHandlerMac
    : public RecoveryComponentActionHandler {
 public:
  RecoveryComponentActionHandlerMac() = default;
  RecoveryComponentActionHandlerMac(const RecoveryComponentActionHandlerMac&) =
      delete;
  RecoveryComponentActionHandlerMac& operator=(
      const RecoveryComponentActionHandlerMac&) = delete;

 private:
  ~RecoveryComponentActionHandlerMac() override = default;

  // Overrides for RecoveryComponentActionHandler.
  base::CommandLine MakeCommandLine(
      const base::FilePath& unpack_path) const override;
  void Elevate(Callback callback) override;
};

base::CommandLine RecoveryComponentActionHandlerMac::MakeCommandLine(
    const base::FilePath& unpack_path) const {
  base::CommandLine command_line(unpack_path.Append(kRecoveryExecutableName));
  command_line.AppendSwitchASCII("browser-version",
                                 version_info::GetVersion().GetString());
  command_line.AppendSwitchASCII("sessionid", session_id());
  command_line.AppendSwitchASCII("appguid", base::mac::BaseBundleID());
  return command_line;
}

void RecoveryComponentActionHandlerMac::Elevate(Callback callback) {
  // Elevation is not yet supported on mac.
  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false, 0, 0));
}

}  // namespace

scoped_refptr<update_client::ActionHandler>
RecoveryComponentActionHandler::MakeActionHandler() {
  return base::MakeRefCounted<RecoveryComponentActionHandlerMac>();
}

}  // namespace component_updater
