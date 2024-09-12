// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/recovery_component_installer.h"

#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
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
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/authorization_util.h"
#include "base/mac/scoped_authorizationref.h"
#endif

using content::BrowserThread;

namespace component_updater {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace {

// CRX hash. The extension id is: npdjjkjlcidkjlamlmmdelcjbcpdjocm.
const uint8_t kRecoverySha2Hash[] = {
    0xdf, 0x39, 0x9a, 0x9b, 0x28, 0x3a, 0x9b, 0x0c, 0xbc, 0xc3, 0x4b,
    0x29, 0x12, 0xf3, 0x9e, 0x2c, 0x19, 0x7a, 0x71, 0x4b, 0x0a, 0x7c,
    0x80, 0x1c, 0xf6, 0x29, 0x7c, 0x0a, 0x5f, 0xea, 0x67, 0xb7};
static_assert(std::size(kRecoverySha2Hash) == crypto::kSHA256Length,
              "Wrong hash length");

// File name of the recovery binary on different platforms.
const base::FilePath::CharType kRecoveryFileName[] =
#if BUILDFLAG(IS_WIN)
    FILE_PATH_LITERAL("ChromeRecovery.exe");
#else  // BUILDFLAG(IS_LINUX), BUILDFLAG(IS_MAC), etc.
    FILE_PATH_LITERAL("ChromeRecovery");
#endif

const char kRecoveryManifestName[] = "ChromeRecovery";

// ChromeRecovery process exit codes.
enum ChromeRecoveryExitCode {
  EXIT_CODE_RECOVERY_SUCCEEDED = 0,
  EXIT_CODE_RECOVERY_SKIPPED = 1,
  EXIT_CODE_ELEVATION_NEEDED = 2,
};

// Checks if elevated recovery simulation switch was present on the command
// line. This is for testing purpose.
bool SimulatingElevatedRecovery() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSimulateElevatedRecovery);
}

std::vector<std::string> GetRecoveryInstallArguments(
    const base::Value::Dict& manifest,
    bool is_deferred_run,
    const base::Version& version) {
  std::vector<std::string> arguments;

  // Add a flag for re-attempted install with elevated privilege so that the
  // recovery executable can report back accordingly.
  if (is_deferred_run) {
    arguments.push_back("/deferredrun");
  }

  if (const std::string* recovery_args =
          manifest.FindString("x-recovery-args")) {
    if (base::IsStringASCII(*recovery_args)) {
      arguments.push_back(*recovery_args);
    }
  }
  if (const std::string* recovery_add_version =
          manifest.FindString("x-recovery-add-version")) {
    if (*recovery_add_version == "yes") {
      arguments.push_back("/version");
      arguments.push_back(version.GetString());
    }
  }

  return arguments;
}

base::CommandLine BuildRecoveryInstallCommandLine(
    const base::FilePath& command,
    const base::Value::Dict& manifest,
    bool is_deferred_run,
    const base::Version& version) {
  base::CommandLine command_line(command);

  const auto arguments =
      GetRecoveryInstallArguments(manifest, is_deferred_run, version);
  for (const auto& arg : arguments) {
    command_line.AppendArg(arg);
  }

  return command_line;
}

base::Value::Dict ReadManifest(const base::FilePath& manifest) {
  JSONFileValueDeserializer deserializer(manifest);
  std::string error;
  return std::move(*deserializer.Deserialize(nullptr, &error)).TakeDict();
}

void WaitForElevatedInstallToComplete(base::Process process) {
  const base::TimeDelta kMaxWaitTime = base::Seconds(600);
  process.WaitForExitWithTimeout(kMaxWaitTime, nullptr);
}

void DoElevatedInstallRecoveryComponent(const base::FilePath& path) {
  const base::FilePath main_file = path.Append(kRecoveryFileName);
  const base::FilePath manifest_file =
      path.Append(FILE_PATH_LITERAL("manifest.json"));
  if (!base::PathExists(main_file) || !base::PathExists(manifest_file)) {
    return;
  }

  base::Value::Dict manifest(ReadManifest(manifest_file));
  const std::string* name = manifest.FindString("name");
  if (!name || *name != kRecoveryManifestName) {
    return;
  }
  std::string proposed_version;
  if (const std::string* ptr = manifest.FindString("version")) {
    if (base::IsStringASCII(*ptr)) {
      proposed_version = *ptr;
    }
  }
  const base::Version version(proposed_version);
  if (!version.IsValid()) {
    return;
  }

  const bool is_deferred_run = true;
#if BUILDFLAG(IS_WIN)
  const auto cmdline = BuildRecoveryInstallCommandLine(
      main_file, manifest, is_deferred_run, version);

  base::LaunchOptions options;
  options.start_hidden = true;
  options.elevated = true;
  base::Process process = base::LaunchProcess(cmdline, options);
#elif BUILDFLAG(IS_MAC)
  base::mac::ScopedAuthorizationRef authRef =
      base::mac::AuthorizationCreateToRunAsRoot(nullptr);
  if (!authRef.get()) {
    return;
  }

  const auto arguments =
      GetRecoveryInstallArguments(manifest, is_deferred_run, version);
  // Convert the arguments memory layout to the format required by
  // ExecuteWithPrivilegesAndGetPID(): an array of string pointers
  // that ends with a null pointer.
  std::vector<const char*> raw_string_args;
  for (const auto& arg : arguments) {
    raw_string_args.push_back(arg.c_str());
  }
  raw_string_args.push_back(nullptr);

  pid_t pid = -1;
  const OSStatus status = base::mac::ExecuteWithPrivilegesAndGetPID(
      authRef.get(), main_file.value().c_str(), kAuthorizationFlagDefaults,
      raw_string_args.data(), nullptr, &pid);
  if (status != errAuthorizationSuccess) {
    return;
  }

  // The child process must print its PID in the first line of its STDOUT. See
  // https://cs.chromium.org/chromium/src/base/mac/authorization_util.h?l=8
  // for more details. When |pid| cannot be determined, we are not able to
  // get process exit code, thus bail out early.
  if (pid < 0) {
    return;
  }
  base::Process process = base::Process::Open(pid);
#endif
  // This task joins a process, hence .WithBaseSyncPrimitives().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::WithBaseSyncPrimitives(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&WaitForElevatedInstallToComplete, std::move(process)));
}

void ElevatedInstallRecoveryComponent(const base::FilePath& installer_path) {
  base::ThreadPool::PostTask(
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
               std::unique_ptr<InstallParams> install_params,
               ProgressCallback progress_callback,
               Callback callback) override;

  std::optional<base::FilePath> GetInstalledFile(
      const std::string& file) override;

  bool Uninstall() override;

 private:
  ~RecoveryComponentInstaller() override = default;

  bool DoInstall(const base::FilePath& unpack_path);

  bool RunInstallCommand(const base::CommandLine& cmdline,
                         const base::FilePath& installer_folder) const;

  base::Version current_version_;
  raw_ptr<PrefService> prefs_;
};

void SimulateElevatedRecoveryHelper(PrefService* prefs) {
  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, true);
}

void RecoveryRegisterHelper(ComponentUpdateService* cus, PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Version version(prefs->GetString(prefs::kRecoveryComponentVersion));
  if (!version.IsValid()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  std::vector<uint8_t> public_key_hash;
  public_key_hash.assign(std::begin(kRecoverySha2Hash),
                         std::end(kRecoverySha2Hash));
  if (!cus->RegisterComponent(ComponentRegistration(
          update_client::GetCrxIdFromPublicKeyHash(public_key_hash), "recovery",
          public_key_hash, version, /*fingerprint=*/{},
          /*installer_attributes=*/{}, /*action_handler=*/nullptr,
          new RecoveryComponentInstaller(version, prefs),
          /*requires_network_encryption=*/false,
          /*supports_group_policy_enable_component_updates=*/true,
          /*allow_cached_copies=*/true,
          /*allow_updates_on_metered_connection=*/true,
          /*allow_updates=*/true))) {
    NOTREACHED_IN_MIGRATION() << "Recovery component registration failed.";
  }
}

void RecoveryUpdateVersionHelper(const base::Version& version,
                                 PrefService* prefs) {
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
    const base::Version& version,
    PrefService* prefs)
    : current_version_(version), prefs_(prefs) {}

void RecoveryComponentInstaller::OnUpdateError(int error) {
  NOTREACHED_IN_MIGRATION() << "Recovery component update error: " << error;
}

void WaitForInstallToComplete(base::Process process,
                              const base::FilePath& installer_folder,
                              PrefService* prefs) {
  int installer_exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::Seconds(600);
  if (process.WaitForExitWithTimeout(kMaxWaitTime, &installer_exit_code)) {
    if (installer_exit_code == EXIT_CODE_ELEVATION_NEEDED) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&SetPrefsForElevatedRecoveryInstall,
                                    installer_folder, prefs));
    }
  }
}

bool RecoveryComponentInstaller::RunInstallCommand(
    const base::CommandLine& cmdline,
    const base::FilePath& installer_folder) const {
  base::LaunchOptions options;
#if BUILDFLAG(IS_WIN)
  options.start_hidden = true;
#endif
  base::Process process = base::LaunchProcess(cmdline, options);
  if (!process.IsValid()) {
    return false;
  }

  // Let worker pool thread wait for us so we don't block Chrome shutdown.
  // This task joins a process, hence .WithBaseSyncPrimitives().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::WithBaseSyncPrimitives(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&WaitForInstallToComplete, std::move(process),
                     installer_folder, prefs_));

  // Returns true regardless of install result since from updater service
  // perspective the install is done, even we may need to do elevated
  // install later.
  return true;
}

#if BUILDFLAG(IS_POSIX)
// Sets the POSIX executable permissions on a file
bool SetPosixExecutablePermission(const base::FilePath& path) {
  int permissions = 0;
  if (!base::GetPosixFilePermissions(path, &permissions)) {
    return false;
  }
  const int kExecutableMask = base::FILE_PERMISSION_EXECUTE_BY_USER |
                              base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                              base::FILE_PERMISSION_EXECUTE_BY_OTHERS;
  if ((permissions & kExecutableMask) == kExecutableMask) {
    return true;  // No need to update
  }
  return base::SetPosixFilePermissions(path, permissions | kExecutableMask);
}
#endif  // BUILDFLAG(IS_POSIX)

void RecoveryComponentInstaller::Install(
    const base::FilePath& unpack_path,
    const std::string& /*public_key*/,
    std::unique_ptr<InstallParams> /*install_params*/,
    ProgressCallback /*progress_callback*/,
    update_client::CrxInstaller::Callback callback) {
  auto result = update_client::InstallFunctionWrapper(
      base::BindOnce(&RecoveryComponentInstaller::DoInstall,
                     base::Unretained(this), std::cref(unpack_path)));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

bool RecoveryComponentInstaller::DoInstall(const base::FilePath& unpack_path) {
  std::optional<base::Value::Dict> manifest =
      update_client::ReadManifest(unpack_path);
  if (!manifest.has_value()) {
    return false;
  }
  const std::string* name = manifest->FindString("name");
  if (!name || *name != kRecoveryManifestName) {
    return false;
  }
  const std::string* proposed_version = manifest->FindString("version");
  if (!proposed_version || !base::IsStringASCII(*proposed_version)) {
    return false;
  }
  base::Version version(*proposed_version);
  if (!version.IsValid()) {
    return false;
  }
  if (current_version_.CompareTo(version) >= 0) {
    return false;
  }

  // Passed the basic tests. Copy the installation to a permanent directory.
  base::FilePath path;
  if (!base::PathService::Get(DIR_RECOVERY_BASE, &path)) {
    return false;
  }
  if (!base::PathExists(path) && !base::CreateDirectory(path)) {
    return false;
  }
  path = path.AppendASCII(version.GetString());
  if (base::PathExists(path) && !base::DeletePathRecursively(path)) {
    return false;
  }
  if (!base::Move(unpack_path, path)) {
    DVLOG(1) << "Recovery component move failed.";
    return false;
  }

  base::FilePath main_file = path.Append(kRecoveryFileName);
  if (!base::PathExists(main_file)) {
    return false;
  }

#if BUILDFLAG(IS_POSIX)
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
      main_file, manifest.value(), is_deferred_run, current_version_);

  if (!RunInstallCommand(cmdline, path)) {
    return false;
  }

  current_version_ = version;
  if (prefs_) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&RecoveryUpdateVersionHelper, version, prefs_));
  }
  return true;
}

std::optional<base::FilePath> RecoveryComponentInstaller::GetInstalledFile(
    const std::string& file) {
  return std::nullopt;
}

bool RecoveryComponentInstaller::Uninstall() {
  return false;
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

void RegisterRecoveryComponent(ComponentUpdateService* cus,
                               PrefService* prefs) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (SimulatingElevatedRecovery()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&SimulateElevatedRecoveryHelper, prefs));
  }

  // We delay execute the registration because we are not required in
  // the critical path during browser startup.
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RecoveryRegisterHelper, cus, prefs),
      base::Seconds(6));
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
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
