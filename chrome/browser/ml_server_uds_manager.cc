#include "chrome/browser/ml_server_uds_manager.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"  // Needed for base::Seconds().
#include "chrome/common/chrome_features.h"

#if BUILDFLAG(IS_LINUX)
#include "build/build_config.h"
#endif

namespace {

// No longer using the local binary; we are using Podman.
  
}  // namespace

// Get Singleton instance
MLServerUdsManager& MLServerUdsManager::GetInstance() {
  static MLServerUdsManager instance;
  return instance;
}

// Constructor (Private)
MLServerUdsManager::MLServerUdsManager() = default;

// Destructor (Cleanup process)
MLServerUdsManager::~MLServerUdsManager() {
  StopMLServer();
}

// Start ML Server using a Podman container.
void MLServerUdsManager::StartMLServerIfEnabled() {
  if (!base::FeatureList::IsEnabled(features::kMLServerUdsFeature))
    return;

#if BUILDFLAG(IS_LINUX)
  base::LaunchOptions options;

  // 1. create the shared socket director
  {
    base::CommandLine sockmkdir_cmdline(base::FilePath("mkdir"));
    sockmkdir_cmdline.AppendArg("-p");
    sockmkdir_cmdline.AppendArg("/tmp/shared-sockets");
    
    // launch the mkdir process.
    base::Process sockmkdir_process = base::LaunchProcess(sockmkdir_cmdline, options);

    if (!sockmkdir_process.IsValid()) {
      LOG(ERROR) << "ML_UDS: Failed to launch mkdir creation";
      return;
    }
    int sockmkdir_exit_code = 0;
    if (!sockmkdir_process.WaitForExitWithTimeout(base::Seconds(300), &sockmkdir_exit_code) ||
        sockmkdir_exit_code != 0) {
      LOG(ERROR) << "ML_UDS: mkdir creation failed with exit code: " << sockmkdir_exit_code;
      return;
    }
  }

  // 2. Build the image using Podman.
  {
    base::CommandLine build_cmdline(base::FilePath("podman"));
    build_cmdline.AppendArg("build");
    build_cmdline.AppendArg("-t");
    build_cmdline.AppendArg("unix-socket-server");
    build_cmdline.AppendArg("/home/bivas_lappy/Desktop/malabr/src/addition_malabr/modelserver_uds");

    // Launch the build process.
    base::Process build_process = base::LaunchProcess(build_cmdline, options);
    if (!build_process.IsValid()) {
      LOG(ERROR) << "ML_UDS: Failed to launch podman build";
      return;
    }
    int build_exit_code = 0;
    if (!build_process.WaitForExitWithTimeout(base::Seconds(300), &build_exit_code) ||
        build_exit_code != 0) {
      LOG(ERROR) << "ML_UDS: Podman build failed with exit code: " << build_exit_code;
      return;
    }
  }

  // 3. Run the container.
  {
    base::CommandLine run_cmdline(base::FilePath("podman"));
    run_cmdline.AppendArg("run");
    run_cmdline.AppendArg("--rm");
    run_cmdline.AppendArg("-d");
    run_cmdline.AppendArg("--name");
    run_cmdline.AppendArg("unix-server");
    run_cmdline.AppendArg("-v");
    run_cmdline.AppendArg("/tmp/shared-sockets:/sockets:Z");
    run_cmdline.AppendArg("unix-socket-server");

    // Launch the container (in detached mode).
    base::Process run_process = base::LaunchProcess(run_cmdline, options);
    if (!run_process.IsValid()) {
      LOG(ERROR) << "ML_UDS: Failed to launch podman container";
      return;
    }
    // Move the process into our member variable.
    ml_server_uds_process_ = std::make_unique<base::Process>(std::move(run_process));
  }
#endif
}

// Stop ML Server: Stop and remove the Podman container.
void MLServerUdsManager::StopMLServer() {
#if BUILDFLAG(IS_LINUX)
  base::LaunchOptions options;

  // 1. Stop the container.
  {
    base::CommandLine stop_cmdline(base::FilePath("podman"));
    stop_cmdline.AppendArg("stop");
    stop_cmdline.AppendArg("unix-server");

    base::Process stop_process = base::LaunchProcess(stop_cmdline, options);
    if (!stop_process.IsValid()) {
      LOG(ERROR) << "ML_UDS: Failed to launch podman stop command";
    } else {
      int stop_exit_code = 0;
      if (!stop_process.WaitForExitWithTimeout(base::Seconds(30), &stop_exit_code) ||
          stop_exit_code != 0) {
        LOG(ERROR) << "ML_UDS: Podman stop command failed with exit code: " << stop_exit_code;
      }
    }
  }

  // 2. Remove the container.
  {
    base::CommandLine rm_cmdline(base::FilePath("podman"));
    rm_cmdline.AppendArg("rm");
    rm_cmdline.AppendArg("unix-server");

    base::Process rm_process = base::LaunchProcess(rm_cmdline, options);
    if (!rm_process.IsValid()) {
      LOG(ERROR) << "ML_UDS: Failed to launch podman rm command";
    } else {
      int rm_exit_code = 0;
      if (!rm_process.WaitForExitWithTimeout(base::Seconds(30), &rm_exit_code) ||
          rm_exit_code != 0) {
        LOG(ERROR) << "ML_UDS: Podman rm command failed with exit code: " << rm_exit_code;
      }
    }
  }

  // Clear any stored process handle.
  ml_server_uds_process_.reset();
#endif
}
