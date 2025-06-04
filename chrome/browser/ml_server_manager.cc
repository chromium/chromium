// #include "chrome/browser/ml_server_manager.h"

// #include "base/command_line.h"
// #include "base/files/file_util.h"
// #include "base/logging.h"
// #include "base/process/launch.h"
// #include "base/process/process.h"
// #include "base/strings/string_number_conversions.h"
// #include "chrome/common/chrome_features.h"

// #if BUILDFLAG(IS_LINUX)
// #include "build/build_config.h"
// #endif

// namespace {

// // Path to ML server binary
// const base::FilePath kServerPath(FILE_PATH_LITERAL("/home/saurav/MTP/server"));

// }  // namespace

// // Get Singleton instance
// MLServerManager& MLServerManager::GetInstance() {
//   static MLServerManager instance;
//   return instance;
// }

// // Constructor (Private)
// MLServerManager::MLServerManager() = default;

// // Destructor (Cleanup process)
// MLServerManager::~MLServerManager() {
//   StopMLServer();
// }

// // Start ML Server
// void MLServerManager::StartMLServerIfEnabled() {
//   if (!base::FeatureList::IsEnabled(features::kMLServerFeature)) return;

// #if BUILDFLAG(IS_LINUX)
//   if (!base::PathExists(kServerPath)) {
//     LOG(ERROR) << "ML server binary not found";
//     return;
//   }

//   int mode = 0;
//   if (!base::GetPosixFilePermissions(kServerPath, &mode)) {
//     LOG(ERROR) << "Failed to retrieve file permissions";
//     return;
//   }
//   if (mode & base::FILE_PERMISSION_WRITE_BY_OTHERS) {
//     LOG(ERROR) << "Insecure ML server permissions";
//     return;
//   }

//   base::CommandLine cmdline(kServerPath);
//   cmdline.AppendSwitchASCII("--browser-pid",
//                             base::NumberToString(base::GetCurrentProcId()));
//   cmdline.AppendSwitch("--no-sandbox");

//   base::LaunchOptions options;
//   options.allow_new_privs = false;

//   ml_server_process_ = std::make_unique<base::Process>(base::LaunchProcess(cmdline, options));
//   if (!ml_server_process_->IsValid()) {
//     LOG(ERROR) << "Failed to launch ML server";
//     ml_server_process_.reset();
//   }
// #endif
// }

// // Stop ML Server
// void MLServerManager::StopMLServer() {
// #if BUILDFLAG(IS_LINUX)
//   if (ml_server_process_ && ml_server_process_->IsValid()) {
//     ml_server_process_->Terminate(/*exit_code=*/0, /*wait=*/false);
//     ml_server_process_.reset();
//   }
// #endif
// }
// ---- Legacy Code above ----

#include "chrome/browser/ml_server_manager.h"

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
MLServerManager& MLServerManager::GetInstance() {
  static MLServerManager instance;
  return instance;
}

// Constructor (Private)
MLServerManager::MLServerManager() = default;

// Destructor (Cleanup process)
MLServerManager::~MLServerManager() {
  StopMLServer();
}

// Start ML Server using a Podman container.
void MLServerManager::StartMLServerIfEnabled() {
  if (!base::FeatureList::IsEnabled(features::kMLServerFeature))
    return;

#if BUILDFLAG(IS_LINUX)
  base::LaunchOptions options;

  // 1. Build the image using Podman.
  {
    base::CommandLine build_cmdline(base::FilePath("podman"));
    build_cmdline.AppendArg("build");
    build_cmdline.AppendArg("-t");
    build_cmdline.AppendArg("mycppserver");
    build_cmdline.AppendArg("/home/bivas_lappy/Desktop/malabr/src/addition_malabr");

    // Launch the build process.
    base::Process build_process = base::LaunchProcess(build_cmdline, options);
    if (!build_process.IsValid()) {
      LOG(ERROR) << "Failed to launch podman build";
      return;
    }
    int build_exit_code = 0;
    if (!build_process.WaitForExitWithTimeout(base::Seconds(300), &build_exit_code) ||
        build_exit_code != 0) {
      LOG(ERROR) << "Podman build failed with exit code: " << build_exit_code;
      return;
    }
  }

  // 2. Run the container.
  {
    base::CommandLine run_cmdline(base::FilePath("podman"));
    run_cmdline.AppendArg("run");
    run_cmdline.AppendArg("-d");
    run_cmdline.AppendArg("-p");
    run_cmdline.AppendArg("5000:5000");
    run_cmdline.AppendArg("-v");
    run_cmdline.AppendArg("/home/bivas_lappy/Desktop/malabr/src/addition_malabr/uploads:/app/uploads:rw");
    run_cmdline.AppendArg("--name");
    run_cmdline.AppendArg("cpp-server-container");
    run_cmdline.AppendArg("mycppserver");

    // Launch the container (in detached mode).
    base::Process run_process = base::LaunchProcess(run_cmdline, options);
    if (!run_process.IsValid()) {
      LOG(ERROR) << "Failed to launch podman container";
      return;
    }
    // Move the process into our member variable.
    ml_server_process_ = std::make_unique<base::Process>(std::move(run_process));
  }
#endif
}

// Stop ML Server: Stop and remove the Podman container.
void MLServerManager::StopMLServer() {
#if BUILDFLAG(IS_LINUX)
  base::LaunchOptions options;

  // 1. Stop the container.
  {
    base::CommandLine stop_cmdline(base::FilePath("podman"));
    stop_cmdline.AppendArg("stop");
    stop_cmdline.AppendArg("cpp-server-container");

    base::Process stop_process = base::LaunchProcess(stop_cmdline, options);
    if (!stop_process.IsValid()) {
      LOG(ERROR) << "Failed to launch podman stop command";
    } else {
      int stop_exit_code = 0;
      if (!stop_process.WaitForExitWithTimeout(base::Seconds(30), &stop_exit_code) ||
          stop_exit_code != 0) {
        LOG(ERROR) << "Podman stop command failed with exit code: " << stop_exit_code;
      }
    }
  }

  // 2. Remove the container.
  {
    base::CommandLine rm_cmdline(base::FilePath("podman"));
    rm_cmdline.AppendArg("rm");
    rm_cmdline.AppendArg("cpp-server-container");

    base::Process rm_process = base::LaunchProcess(rm_cmdline, options);
    if (!rm_process.IsValid()) {
      LOG(ERROR) << "Failed to launch podman rm command";
    } else {
      int rm_exit_code = 0;
      if (!rm_process.WaitForExitWithTimeout(base::Seconds(30), &rm_exit_code) ||
          rm_exit_code != 0) {
        LOG(ERROR) << "Podman rm command failed with exit code: " << rm_exit_code;
      }
    }
  }

  // Clear any stored process handle.
  ml_server_process_.reset();
#endif
}
