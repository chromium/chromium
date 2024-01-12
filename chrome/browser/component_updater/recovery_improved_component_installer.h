// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_RECOVERY_IMPROVED_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_RECOVERY_IMPROVED_COMPONENT_INSTALLER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/unpacker.h"
#include "components/update_client/update_client.h"

namespace base {
class CommandLine;
class Process;
}  // namespace base

class PrefService;

namespace component_updater {

// Handles the |run| action for the recovery component.
//
// The recovery component consists of a executable program (the name of the
// program is known at build time but it depends on the operating system),
// wrapped inside a CRX container, which CRX is itself contained inside a
// component updater CRX payload. In other words, looking from outside, the
// component updater installs and updates a CRX, containing a CRX, containing
// a program. When this program is run, it can restore the functionality of
// the Chrome updater.

// The |RecoveryComponentActionHandler| is responsible for unpacking the inner
// CRX described above, and running the executable program inside it.
//
// The |RecoveryComponentActionHandler::Handle| function is invoked as a result
// of an |action| element present in the update response for the recovery
// component. Note that the |action| element can be present in the update
// response even if there are no updates for the recovery component.
//
// When Chrome is installed per-system, the CRX is being handed over to
// a system elevator, which unpacks the CRX in a secure location of the
// file system, and runs the recovery program with system privileges.
//
// When Chrome is installed per-user, the CRX is unpacked in a temporary
// directory for the user, and the recovery program runs with normal user
// privileges.
class RecoveryComponentActionHandler : public update_client::ActionHandler {
 public:
  static scoped_refptr<update_client::ActionHandler> MakeActionHandler();

  RecoveryComponentActionHandler(const RecoveryComponentActionHandler&) =
      delete;
  RecoveryComponentActionHandler& operator=(
      const RecoveryComponentActionHandler&) = delete;

  // Overrides for update_client::RecoveryComponentActionHandler. |action| is an
  // absolute file path to a CRX to be unpacked. |session_id| contains the
  // session id corresponding to the current update transaction. The session id
  // is passed as a command line argument to the recovery program, and sent as
  // part of the completion pings during the actual recovery.
  void Handle(const base::FilePath& action,
              const std::string& session_id,
              Callback callback) override;

 protected:
  static constexpr base::TaskTraits kThreadPoolTaskTraits = {
      base::MayBlock(), base::TaskPriority::BEST_EFFORT,
      base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

  // This task joins a process, hence .WithBaseSyncPrimitives().
  static constexpr base::TaskTraits kThreadPoolTaskTraitsRunCommand = {
      base::MayBlock(), base::WithBaseSyncPrimitives(),
      base::TaskPriority::BEST_EFFORT,
      base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

  // Accepts only the production key hash and the production proof by default.
  RecoveryComponentActionHandler(
      const std::vector<uint8_t>& key_hash = {std::begin(kKeyHash),
                                              std::end(kKeyHash)},
      crx_file::VerifierFormat verifier_format =
          crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF);
  ~RecoveryComponentActionHandler() override;

  base::FilePath crx_path() const { return crx_path_; }

  std::string session_id() const { return session_id_; }

  scoped_refptr<base::SequencedTaskRunner> main_task_runner() {
    return main_task_runner_;
  }

 private:
  // The production public key hash for the inner CRX.
  static constexpr uint8_t kKeyHash[] = {
      0x5f, 0x94, 0xe0, 0x3c, 0x64, 0x30, 0x9f, 0xbc, 0xfe, 0x00, 0x9a,
      0x27, 0x3e, 0x52, 0xbf, 0xa5, 0x84, 0xb9, 0xb3, 0x75, 0x07, 0x29,
      0xde, 0xfa, 0x32, 0x76, 0xd9, 0x93, 0xb5, 0xa3, 0xce, 0x02};

  virtual base::CommandLine MakeCommandLine(
      const base::FilePath& unpack_path) const = 0;
  virtual void PrepareFiles(const base::FilePath& unpack_path) const = 0;
  virtual void Elevate(Callback callback) = 0;

  void Unpack();
  void UnpackComplete(const update_client::Unpacker::Result& result);
  void RunCommand(const base::CommandLine& cmdline);

  // `process` contains the process object, if the process was successfully
  // created or an error value otherwise (if the error is available on that
  // platform).
  void WaitForCommand(base::expected<base::Process, int> process_or_error);

  SEQUENCE_CHECKER(sequence_checker_);

  // Executes tasks in the context of the sequence which created this object.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();

  // The key hash and its proof for the inner CRX to be unpacked and run.
  const std::vector<uint8_t> key_hash_;
  const crx_file::VerifierFormat verifier_format_;

  // Contains the CRX specified as a run action.
  base::FilePath crx_path_;

  // The session id of the update transaction, as defined by |update_client|.
  std::string session_id_;

  // Called when the action is handled.
  Callback callback_;

  // Contains the path where the action CRX is unpacked in the per-user case.
  base::FilePath unpack_path_;
};

class ComponentUpdateService;

class RecoveryImprovedInstallerPolicy : public ComponentInstallerPolicy {
 public:
  explicit RecoveryImprovedInstallerPolicy(PrefService* prefs)
      : prefs_(prefs) {}
  ~RecoveryImprovedInstallerPolicy() override = default;
  RecoveryImprovedInstallerPolicy(const RecoveryImprovedInstallerPolicy&) =
      delete;
  RecoveryImprovedInstallerPolicy& operator=(
      const RecoveryImprovedInstallerPolicy&) = delete;

 private:
  friend class RecoveryImprovedInstallerTest;

  // ComponentInstallerPolicy implementation.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  raw_ptr<PrefService> prefs_;
};

void RegisterRecoveryImprovedComponent(ComponentUpdateService* cus,
                                       PrefService* prefs);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_RECOVERY_IMPROVED_COMPONENT_INSTALLER_H_
