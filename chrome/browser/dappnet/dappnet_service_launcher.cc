#include "chrome/browser/dappnet/dappnet_service_launcher.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace chrome {

namespace {

// Port for the DappNet service to listen on
constexpr int kDappNetServicePort = 10422;

// Binary name for the service
#if BUILDFLAG(IS_WIN)
constexpr char kDappNetServiceBinary[] = "local-gateway.exe";
#else
constexpr char kDappNetServiceBinary[] = "local-gateway";
#endif

}  // namespace

DappNetServiceLauncher::DappNetServiceLauncher() {
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({
      base::TaskPriority::BEST_EFFORT,
      base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
  });
}

DappNetServiceLauncher::~DappNetServiceLauncher() {
  Stop();
}

bool DappNetServiceLauncher::Start() {
  if (IsRunning()) {
    DVLOG(1) << "DappNet service already running";
    return true;
  }

  // Get the path to the service binary (should be in the same directory as Chrome)
  base::FilePath exe_path = base::GetProcessExecutablePath(base::GetCurrentProcId());
  if (exe_path.empty()) {
    LOG(ERROR) << "Could not get executable path";
    return false;
  }
  base::FilePath exe_dir = exe_path.DirName();

  base::FilePath service_path = exe_dir.Append(kDappNetServiceBinary);

  // Build command line
  base::CommandLine cmd(service_path);
  cmd.AppendSwitchASCII("port", base::NumberToString(kDappNetServicePort));
  cmd.AppendSwitchASCII("parent-pid", 
                        base::NumberToString(base::GetCurrentProcId()));

  // Launch options
  base::LaunchOptions options;
#if BUILDFLAG(IS_WIN)
  options.start_hidden = true;
#endif
  
  // Detach the process so it doesn't become a zombie
  options.new_process_group = true;

  LOG(INFO) << "Launching DappNet service: " << cmd.GetCommandLineString();

  // Launch the process
  base::Process process = base::LaunchProcess(cmd, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch DappNet service";
    return false;
  }

  service_pid_ = process.Pid();
  
  // Detach from the process (it will run independently)
  base::IgnoreResult(process.Release());

  LOG(INFO) << "DappNet service started with PID: " << service_pid_;
  return true;
}

void DappNetServiceLauncher::Stop() {
  if (!IsRunning()) {
    return;
  }

  // On Unix systems, we can try to terminate the process gracefully
#if BUILDFLAG(IS_POSIX)
  if (service_pid_ > 0) {
    LOG(INFO) << "Stopping DappNet service (PID: " << service_pid_ << ")";
    
    // Try SIGTERM first
    if (kill(service_pid_, SIGTERM) == 0) {
      // Give it a moment to shut down gracefully
      usleep(100000);  // 100ms
      
      // Check if it's still running
      if (kill(service_pid_, 0) == 0) {
        // Still running, force kill
        kill(service_pid_, SIGKILL);
      }
    }
  }
#endif

  service_pid_ = 0;
}

bool DappNetServiceLauncher::IsRunning() const {
  if (service_pid_ <= 0) {
    return false;
  }

#if BUILDFLAG(IS_POSIX)
  // Check if the process is still alive
  return kill(service_pid_, 0) == 0;
#elif BUILDFLAG(IS_WIN)
  // On Windows, try to open the process handle
  HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, service_pid_);
  if (process_handle != NULL) {
    DWORD exit_code;
    bool running = GetExitCodeProcess(process_handle, &exit_code) && 
                   exit_code == STILL_ACTIVE;
    CloseHandle(process_handle);
    return running;
  }
  return false;
#else
  // Fallback - assume it's running
  return true;
#endif
}

// Global instance
DappNetServiceLauncher* GetDappNetServiceLauncher() {
  static base::NoDestructor<DappNetServiceLauncher> instance;
  return instance.get();
}

}  // namespace chrome