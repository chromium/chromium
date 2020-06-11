// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lacros/lacros_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/lacros/lacros_util.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/session_manager/core/session_manager.h"
#include "google_apis/google_api_keys.h"

using component_updater::CrOSComponentManager;

namespace {

LacrosLoader* g_instance = nullptr;

const char kLacrosComponentName[] = "lacros-fishfood";
const char kUserDataDir[] = "/home/chronos/user/lacros";

bool CheckIfPreviouslyInstalled(
    scoped_refptr<component_updater::CrOSComponentManager> manager) {
  if (!manager->IsRegisteredMayBlock(kLacrosComponentName))
    return false;

  // Since we're already on a background thread, delete the user-data-dir
  // associated with lacros.
  base::DeleteFileRecursively(base::FilePath(kUserDataDir));
  return true;
}
}  // namespace

// static
LacrosLoader* LacrosLoader::Get() {
  return g_instance;
}

LacrosLoader::LacrosLoader(scoped_refptr<CrOSComponentManager> manager)
    : cros_component_manager_(manager) {
  DCHECK(!g_instance);
  g_instance = this;

  // Wait to query the flag until the user has entered the session. Enterprise
  // devices restart Chrome during login to apply flags. We don't want to run
  // the flag-off cleanup logic until we know we have the final flag state.
  session_manager::SessionManager::Get()->AddObserver(this);
}

LacrosLoader::~LacrosLoader() {
  session_manager::SessionManager::Get()->RemoveObserver(this);

  // Try to kill the lacros-chrome binary.
  if (lacros_process_.IsValid())
    lacros_process_.Terminate(/*ignored=*/0, /*wait=*/false);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

bool LacrosLoader::IsReady() const {
  return !lacros_path_.empty();
}

void LacrosLoader::SetLoadCompleteCallback(LoadCompleteCallback callback) {
  load_complete_callback_ = std::move(callback);
}

void LacrosLoader::Start() {
  if (!lacros_util::IsLacrosAllowed())
    return;

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LacrosLoader::StartBackground, base::Unretained(this)),
      base::BindOnce(&LacrosLoader::StartForeground, base::Unretained(this)));
}

void LacrosLoader::OnUserSessionStarted(bool is_primary_user) {
  // Ensure this isn't called multiple times.
  session_manager::SessionManager::Get()->RemoveObserver(this);

  // Must be checked after user session start because it depends on user type.
  if (!lacros_util::IsLacrosAllowed())
    return;

  // May be null in tests.
  if (!cros_component_manager_)
    return;

  if (chromeos::features::IsLacrosSupportEnabled()) {
    // TODO(crbug.com/1078607): Remove non-error logging from this class.
    LOG(WARNING) << "Starting lacros component load.";

    // If the user has specified a path for the lacros-chrome binary, use that
    // rather than component manager.
    base::FilePath lacros_chrome_path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            chromeos::switches::kLacrosChromePath);
    if (!lacros_chrome_path.empty()) {
      OnLoadComplete(CrOSComponentManager::Error::NONE, lacros_chrome_path);
      return;
    }

    cros_component_manager_->Load(kLacrosComponentName,
                                  CrOSComponentManager::MountPolicy::kMount,
                                  CrOSComponentManager::UpdatePolicy::kForce,
                                  base::BindOnce(&LacrosLoader::OnLoadComplete,
                                                 weak_factory_.GetWeakPtr()));
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&CheckIfPreviouslyInstalled, cros_component_manager_),
        base::BindOnce(&LacrosLoader::CleanUp, weak_factory_.GetWeakPtr()));
  }
}

bool LacrosLoader::StartBackground() {
  // TODO(erikchen): If Lacros is already running, then we should send a mojo
  // signal to open a new tab rather than going through the start flow again.
  bool already_running = IsLacrosRunning();

  if (!already_running) {
    // Only delete the old log file if lacros is not running. If it's already
    // running, then the subsequent call to base::LaunchProcess opens a new
    // window, and we do not want to delete the existing log file.
    // TODO(erikchen): Currently, launching a second instance of chrome deletes
    // the existing log file, even though the new instance quickly exits.
    base::DeleteFile(base::FilePath(LogPath()), /*recursive=*/false);
  }

  return already_running;
}

void LacrosLoader::StartForeground(bool already_running) {
  // TODO(jamescook): Provide a switch to override the lacros-chrome path for
  // developers.
  if (lacros_path_.empty()) {
    LOG(WARNING) << "lacros component image not yet available";
    return;
  }

  std::string chrome_path = lacros_path_.MaybeAsASCII() + "/chrome";
  LOG(WARNING) << "Launching lacros-chrome at " << chrome_path;

  base::LaunchOptions options;
  options.environment["EGL_PLATFORM"] = "surfaceless";
  options.environment["XDG_RUNTIME_DIR"] = "/run/chrome";

  std::string api_key;
  if (google_apis::HasAPIKeyConfigured())
    api_key = google_apis::GetAPIKey();
  else
    api_key = google_apis::GetNonStableAPIKey();
  options.environment["GOOGLE_API_KEY"] = api_key;
  options.environment["GOOGLE_DEFAULT_CLIENT_ID"] =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  options.environment["GOOGLE_DEFAULT_CLIENT_SECRET"] =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);

  options.kill_on_parent_death = true;

  std::vector<std::string> argv = {
      chrome_path,
      "--ozone-platform=wayland",
      std::string("--user-data-dir=") + kUserDataDir,
      "--enable-gpu-rasterization",
      "--enable-oop-rasterization",
      "--lang=en-US",
      "--breakpad-dump-location=/tmp"};

  // We assume that if there's a custom chrome path, that this is a developer
  // and they want to enable logging.
  bool custom_chrome_path = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kLacrosChromePath);
  if (custom_chrome_path) {
    argv.push_back("--enable-logging");
    argv.push_back(std::string("--log-file=") + LogPath());
  }

  // If Lacros is already running, then the new call to launch process spawns a
  // new window but does not create a lasting process.
  if (already_running) {
    base::LaunchProcess(argv, options);
  } else {
    base::RecordAction(base::UserMetricsAction("Lacros.Launch"));
    lacros_process_ = base::LaunchProcess(argv, options);
  }
  LOG(WARNING) << "Launched lacros-chrome with pid " << lacros_process_.Pid();
}

// static
std::string LacrosLoader::LogPath() {
  std::string log_file(kUserDataDir);
  return log_file + "/lacros.log";
}

void LacrosLoader::OnLoadComplete(
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  bool success = (error == CrOSComponentManager::Error::NONE);
  if (success) {
    lacros_path_ = path;
    LOG(WARNING) << "Loaded lacros image at " << lacros_path_.MaybeAsASCII();
  } else {
    LOG(WARNING) << "Error loading lacros component image: "
                 << static_cast<int>(error);
  }
  if (load_complete_callback_)
    std::move(load_complete_callback_).Run(success);
}

void LacrosLoader::CleanUp(bool previously_installed) {
  if (previously_installed)
    cros_component_manager_->Unload(kLacrosComponentName);
}

bool LacrosLoader::IsLacrosRunning() {
  // TODO(https://crbug.com/1091863): This logic is not robust against the
  // situation where Lacros has been killed, but another process was spawned
  // with the same pid. This logic also relies on I/O, which we'd like to avoid
  // if possible.
  if (!lacros_process_.IsValid())
    return false;
  // We avoid using WaitForExitWithTimeout() since that can block for up to
  // 256ms. Instead, we check existence of /proc/<pid>/cmdline and check for a
  // match against lacros_path_. This logic assumes that lacros_path_ is a fully
  // qualified path.
  base::FilePath cmdline_filepath("/proc");
  cmdline_filepath =
      cmdline_filepath.Append(base::NumberToString(lacros_process_.Pid()));
  cmdline_filepath = cmdline_filepath.Append("cmdline");
  base::File cmdline_file = base::File(
      cmdline_filepath, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!cmdline_file.IsValid())
    return false;
  std::string expected_cmdline = lacros_path_.value();
  size_t expected_length = expected_cmdline.size();
  char data[1000];
  int size_read = cmdline_file.Read(0, data, 1000);
  if (static_cast<size_t>(size_read) < expected_length)
    return false;
  return expected_cmdline.compare(0, expected_length, data, expected_length) ==
         0;
}
