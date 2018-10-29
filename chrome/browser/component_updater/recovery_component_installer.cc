// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/recovery_component_installer.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#if defined(OS_MACOSX)
#include "base/mac/authorization_util.h"
#include "base/mac/scoped_authorizationref.h"
#endif
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/pref_names.h"
#include "components/crx_file/crx_verifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client.h"
#include "components/update_client/utils.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

using content::BrowserThread;

namespace component_updater {

#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_WIN) || defined(OS_MACOSX)

namespace {

// CRX hash. The extension id is: npdjjkjlcidkjlamlmmdelcjbcpdjocm.
const uint8_t kRecoverySha2Hash[] = {
    0xdf, 0x39, 0x9a, 0x9b, 0x28, 0x3a, 0x9b, 0x0c, 0xbc, 0xc3, 0x4b,
    0x29, 0x12, 0xf3, 0x9e, 0x2c, 0x19, 0x7a, 0x71, 0x4b, 0x0a, 0x7c,
    0x80, 0x1c, 0xf6, 0x29, 0x7c, 0x0a, 0x5f, 0xea, 0x67, 0xb7};
static_assert(arraysize(kRecoverySha2Hash) == crypto::kSHA256Length,
              "Wrong hash length");

// File name of the recovery binary on different platforms.
const base::FilePath::CharType kRecoveryFileName[] =
#if defined(OS_WIN)
    FILE_PATH_LITERAL("ChromeRecovery.exe");
#else  // OS_LINUX, OS_MACOSX, etc.
    FILE_PATH_LITERAL("ChromeRecovery");
#endif

const char kRecoveryManifestName[] = "ChromeRecovery";

// ChromeRecovery process exit codes.
enum ChromeRecoveryExitCode {
  EXIT_CODE_RECOVERY_SUCCEEDED = 0,
  EXIT_CODE_RECOVERY_SKIPPED = 1,
  EXIT_CODE_ELEVATION_NEEDED = 2,
};

enum RecoveryComponentEvent {
  RCE_RUNNING_NON_ELEVATED = 0,
  RCE_ELEVATION_NEEDED = 1,
  RCE_FAILED = 2,
  RCE_SUCCEEDED = 3,
  RCE_SKIPPED = 4,
  RCE_RUNNING_ELEVATED = 5,
  RCE_ELEVATED_FAILED = 6,
  RCE_ELEVATED_SUCCEEDED = 7,
  RCE_ELEVATED_SKIPPED = 8,
  RCE_COMPONENT_DOWNLOAD_ERROR = 9,
  RCE_ELEVATED_UNKNOWN_RESULT = 10,
  RCE_COUNT
};

void RecordRecoveryComponentUMAEvent(RecoveryComponentEvent event) {
  UMA_HISTOGRAM_ENUMERATION("RecoveryComponent.Event", event, RCE_COUNT);
}

// Checks if elevated recovery simulation switch was present on the command
// line. This is for testing purpose.
bool SimulatingElevatedRecovery() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSimulateElevatedRecovery);
}

std::vector<std::string> GetRecoveryInstallArguments(
    const base::DictionaryValue& manifest,
    bool is_deferred_run,
    const base::Version& version) {
  std::vector<std::string> arguments;

  // Add a flag for re-attempted install with elevated privilege so that the
  // recovery executable can report back accordingly.
  if (is_deferred_run)
    arguments.push_back("/deferredrun");

  std::string recovery_args;
  if (manifest.GetStringASCII("x-recovery-args", &recovery_args))
    arguments.push_back(recovery_args);
  std::string recovery_add_version;
  if (manifest.GetStringASCII("x-recovery-add-version",
                              &recovery_add_version) &&
      recovery_add_version == "yes") {
    arguments.push_back("/version");
    arguments.push_back(version.GetString());
  }

  return arguments;
}

base::CommandLine BuildRecoveryInstallCommandLine(
    const base::FilePath& command,
    const base::DictionaryValue& manifest,
    bool is_deferred_run,
    const base::Version& version) {
  base::CommandLine command_line(command);

  const auto arguments = GetRecoveryInstallArguments(
      manifest, is_deferred_run, version);
  for (const auto& arg : arguments)
    command_line.AppendArg(arg);

  return command_line;
}

std::unique_ptr<base::DictionaryValue> ReadManifest(
    const base::FilePath& manifest) {
  JSONFileValueDeserializer deserializer(manifest);
  std::string error;
  return base::DictionaryValue::From(deserializer.Deserialize(NULL, &error));
}

void WaitForElevatedInstallToComplete(base::Process process) {
  int installer_exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(600);
  if (process.WaitForExitWithTimeout(kMaxWaitTime, &installer_exit_code)) {
    if (installer_exit_code == EXIT_CODE_RECOVERY_SUCCEEDED) {
      RecordRecoveryComponentUMAEvent(RCE_ELEVATED_SUCCEEDED);
    } else {
      RecordRecoveryComponentUMAEvent(RCE_ELEVATED_SKIPPED);
    }
  } else {
    RecordRecoveryComponentUMAEvent(RCE_ELEVATED_FAILED);
  }
}

void DoElevatedInstallRecoveryComponent(const base::FilePath& path) {
  const base::FilePath main_file = path.Append(kRecoveryFileName);
  const base::FilePath manifest_file =
      path.Append(FILE_PATH_LITERAL("manifest.json"));
  if (!base::PathExists(main_file) || !base::PathExists(manifest_file))
    return;

  std::unique_ptr<base::DictionaryValue> manifest(ReadManifest(manifest_file));
  std::string name;
  manifest->GetStringASCII("name", &name);
  if (name != kRecoveryManifestName)
    return;
  std::string proposed_version;
  manifest->GetStringASCII("version", &proposed_version);
  const base::Version version(proposed_version);
  if (!version.IsValid())
    return;

  const bool is_deferred_run = true;
#if defined(OS_WIN)
  const auto cmdline = BuildRecoveryInstallCommandLine(
      main_file, *manifest, is_deferred_run, version);

  RecordRecoveryComponentUMAEvent(RCE_RUNNING_ELEVATED);

  base::LaunchOptions options;
  options.start_hidden = true;
  base::Process process = base::LaunchElevatedProcess(cmdline, options);
#elif defined(OS_MACOSX)
  base::mac::ScopedAuthorizationRef authRef(
      base::mac::AuthorizationCreateToRunAsRoot(nullptr));
  if (!authRef.get()) {
    RecordRecoveryComponentUMAEvent(RCE_ELEVATED_FAILED);
    return;
  }

  const auto arguments = GetRecoveryInstallArguments(
      *manifest, is_deferred_run, version);
  // Convert the arguments memory layout to the format required by
  // ExecuteWithPrivilegesAndGetPID(): an array of string pointers
  // that ends with a null pointer.
  std::vector<const char*> raw_string_args;
  for (const auto& arg : arguments)
    raw_string_args.push_back(arg.c_str());
  raw_string_args.push_back(nullptr);

  pid_t pid = -1;
  const OSStatus status = base::mac::ExecuteWithPrivilegesAndGetPID(
      authRef.get(), main_file.value().c_str(), kAuthorizationFlagDefaults,
      raw_string_args.data(), nullptr, &pid);
  if (status != errAuthorizationSuccess) {
    RecordRecoveryComponentUMAEvent(RCE_ELEVATED_FAILED);
    return;
  }

  // The child process must print its PID in the first line of its STDOUT. See
  // https://cs.chromium.org/chromium/src/base/mac/authorization_util.h?l=8
  // for more details. When |pid| cannot be determined, we are not able to
  // get process exit code, thus bail out early.
  if (pid < 0) {
    RecordRecoveryComponentUMAEvent(RCE_ELEVATED_UNKNOWN_RESULT);
    return;
  }
  base::Process process = base::Process::Open(pid);
#endif
  // This task joins a process, hence .WithBaseSyncPrimitives().
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::WithBaseSyncPrimitives(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&WaitForElevatedInstallToComplete,
                     base::Passed(&process)));
}

void ElevatedInstallRecoveryComponent(const base::FilePath& installer_path) {
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DoElevatedInstallRecoveryComponent, installer_path));
}

}  // namespace

// Component installer that is responsible to repair the chrome installation
// or repair the Google update installation. This is a last resort safety
// mechanism.
// For user Chrome, recovery component just installs silently. For machine
// Chrome, elevation may be needed. If that happens, the installer will set
// preference flag prefs::kRecoveryComponentNeedsElevation to request that.
// There is a global error service monitors this flag and will pop up
// bubble if the flag is set to true.
// See chrome/browser/recovery/recovery_install_global_error.cc for details.
class RecoveryComponentInstaller : public update_client::CrxInstaller {
 public:
  RecoveryComponentInstaller(const base::Version& version, PrefService* prefs);

  // ComponentInstaller implementation:
  void OnUpdateError(int error) override;

  void Install(const base::FilePath& unpack_path,
               const std::string& public_key,
               Callback callback) override;

  bool GetInstalledFile(const std::string& file,
                        base::FilePath* installed_file) override;

  bool Uninstall() override;

 private:
  ~RecoveryComponentInstaller() override {}

  bool DoInstall(const base::FilePath& unpack_path);

  bool RunInstallCommand(const base::CommandLine& cmdline,
                         const base::FilePath& installer_folder) const;

  base::Version current_version_;
  PrefService* prefs_;
};

void SimulateElevatedRecoveryHelper(PrefService* prefs) {
  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, true);
}

void RecoveryRegisterHelper(ComponentUpdateService* cus, PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Version version(prefs->GetString(prefs::kRecoveryComponentVersion));
  if (!version.IsValid()) {
    NOTREACHED();
    return;
  }
  update_client::CrxComponent recovery;
  recovery.name = "recovery";
  recovery.installer = new RecoveryComponentInstaller(version, prefs);
  recovery.version = version;
  recovery.pk_hash.assign(kRecoverySha2Hash,
                          &kRecoverySha2Hash[sizeof(kRecoverySha2Hash)]);
  recovery.supports_group_policy_enable_component_updates = true;
  recovery.requires_network_encryption = false;
  recovery.crx_format_requirement =
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF;
  if (!cus->RegisterComponent(recovery)) {
    NOTREACHED() << "Recovery component registration failed.";
  }
}

void RecoveryUpdateVersionHelper(
    const base::Version& version, PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  prefs->SetString(prefs::kRecoveryComponentVersion, version.GetString());
}

void SetPrefsForElevatedRecoveryInstall(const base::FilePath& unpack_path,
                                        PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  prefs->SetFilePath(prefs::kRecoveryComponentUnpackPath, unpack_path);
  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, true);
}

RecoveryComponentInstaller::RecoveryComponentInstaller(
    const base::Version& version, PrefService* prefs)
    : current_version_(version), prefs_(prefs) {
  DCHECK(version.IsValid());
}

void RecoveryComponentInstaller::OnUpdateError(int error) {
  RecordRecoveryComponentUMAEvent(RCE_COMPONENT_DOWNLOAD_ERROR);
  NOTREACHED() << "Recovery component update error: " << error;
}

void WaitForInstallToComplete(base::Process process,
                              const base::FilePath& installer_folder,
                              PrefService* prefs) {
  int installer_exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(600);
  if (process.WaitForExitWithTimeout(kMaxWaitTime, &installer_exit_code)) {
    if (installer_exit_code == EXIT_CODE_ELEVATION_NEEDED) {
      RecordRecoveryComponentUMAEvent(RCE_ELEVATION_NEEDED);

      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&SetPrefsForElevatedRecoveryInstall, installer_folder,
                         prefs));
    } else if (installer_exit_code == EXIT_CODE_RECOVERY_SUCCEEDED) {
      RecordRecoveryComponentUMAEvent(RCE_SUCCEEDED);
    } else if (installer_exit_code == EXIT_CODE_RECOVERY_SKIPPED) {
      RecordRecoveryComponentUMAEvent(RCE_SKIPPED);
    }
  } else {
    RecordRecoveryComponentUMAEvent(RCE_FAILED);
  }
}

bool RecoveryComponentInstaller::RunInstallCommand(
    const base::CommandLine& cmdline,
    const base::FilePath& installer_folder) const {
  RecordRecoveryComponentUMAEvent(RCE_RUNNING_NON_ELEVATED);

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
#endif
  base::Process process = base::LaunchProcess(cmdline, options);
  if (!process.IsValid())
    return false;

  // Let worker pool thread wait for us so we don't block Chrome shutdown.
  // This task joins a process, hence .WithBaseSyncPrimitives().
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::WithBaseSyncPrimitives(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&WaitForInstallToComplete, base::Passed(&process),
                     installer_folder, prefs_));

  // Returns true regardless of install result since from updater service
  // perspective the install is done, even we may need to do elevated
  // install later.
  return true;
}

#if defined(OS_POSIX)
// Sets the POSIX executable permissions on a file
bool SetPosixExecutablePermission(const base::FilePath& path) {
  int permissions = 0;
  if (!base::GetPosixFilePermissions(path, &permissions))
    return false;
  const int kExecutableMask = base::FILE_PERMISSION_EXECUTE_BY_USER |
                              base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                              base::FILE_PERMISSION_EXECUTE_BY_OTHERS;
  if ((permissions & kExecutableMask) == kExecutableMask)
    return true;  // No need to update
  return base::SetPosixFilePermissions(path, permissions | kExecutableMask);
}
#endif  // defined(OS_POSIX)

void RecoveryComponentInstaller::Install(
    const base::FilePath& unpack_path,
    const std::string& /*public_key*/,
    update_client::CrxInstaller::Callback callback) {
  auto result = update_client::InstallFunctionWrapper(
      base::BindOnce(&RecoveryComponentInstaller::DoInstall,
                     base::Unretained(this), base::ConstRef(unpack_path)));
  base::PostTask(FROM_HERE, base::BindOnce(std::move(callback), result));
}

bool RecoveryComponentInstaller::DoInstall(
    const base::FilePath& unpack_path) {
  const auto manifest = update_client::ReadManifest(unpack_path);
  if (!manifest)
    return false;
  std::string name;
  manifest->GetStringASCII("name", &name);
  if (name != kRecoveryManifestName)
    return false;
  std::string proposed_version;
  manifest->GetStringASCII("version", &proposed_version);
  base::Version version(proposed_version);
  if (!version.IsValid())
    return false;
  if (current_version_.CompareTo(version) >= 0)
    return false;

  // Passed the basic tests. Copy the installation to a permanent directory.
  base::FilePath path;
  if (!base::PathService::Get(DIR_RECOVERY_BASE, &path))
    return false;
  if (!base::PathExists(path) && !base::CreateDirectory(path))
    return false;
  path = path.AppendASCII(version.GetString());
  if (base::PathExists(path) && !base::DeleteFile(path, true))
    return false;
  if (!base::Move(unpack_path, path)) {
    DVLOG(1) << "Recovery component move failed.";
    return false;
  }

  base::FilePath main_file = path.Append(kRecoveryFileName);
  if (!base::PathExists(main_file))
    return false;

#if defined(OS_POSIX)
  // The current version of the CRX unzipping does not restore
  // correctly the executable flags/permissions. See https://crbug.com/555011
  if (!SetPosixExecutablePermission(main_file)) {
    DVLOG(1) << "Recovery component failed to set the executable "
                "permission on the file: "
             << main_file.value();
    return false;
  }
#endif

  // Run the recovery component.
  const bool is_deferred_run = false;
  const auto cmdline = BuildRecoveryInstallCommandLine(
      main_file, *manifest, is_deferred_run, current_version_);

  if (!RunInstallCommand(cmdline, path)) {
    return false;
  }

  current_version_ = version;
  if (prefs_) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&RecoveryUpdateVersionHelper, version, prefs_));
  }
  return true;
}

bool RecoveryComponentInstaller::GetInstalledFile(
    const std::string& file,
    base::FilePath* installed_file) {
  return false;
}

bool RecoveryComponentInstaller::Uninstall() {
  return false;
}

#endif  // defined(OS_WIN) || defined(OS_MACOSX)
#endif  // defined(GOOGLE_CHROME_BUILD)

void RegisterRecoveryComponent(ComponentUpdateService* cus,
                               PrefService* prefs) {
#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_WIN) || defined(OS_MACOSX)
  if (SimulatingElevatedRecovery()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&SimulateElevatedRecoveryHelper, prefs));
  }

  // We delay execute the registration because we are not required in
  // the critical path during browser startup.
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&RecoveryRegisterHelper, cus, prefs),
      base::TimeDelta::FromSeconds(6));
#endif
#endif
}

void RegisterPrefsForRecoveryComponent(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kRecoveryComponentVersion, "0.0.0.0");
  registry->RegisterFilePathPref(prefs::kRecoveryComponentUnpackPath,
                                 base::FilePath());
  registry->RegisterBooleanPref(prefs::kRecoveryComponentNeedsElevation, false);
}

void AcceptedElevatedRecoveryInstall(PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_WIN) || defined(OS_MACOSX)
  ElevatedInstallRecoveryComponent(
      prefs->GetFilePath(prefs::kRecoveryComponentUnpackPath));
#endif
#endif

  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, false);
}

void DeclinedElevatedRecoveryInstall(PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, false);
}

}  // namespace component_updater
