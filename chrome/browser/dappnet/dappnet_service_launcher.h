#ifndef CHROME_BROWSER_DAPPNET_DAPPNET_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_DAPPNET_DAPPNET_SERVICE_LAUNCHER_H_

#include "base/memory/scoped_refptr.h"

namespace base {
class SequencedTaskRunner;
}

namespace chrome {

// Launches and manages the external DappNet service process.
class DappNetServiceLauncher {
 public:
  DappNetServiceLauncher();
  ~DappNetServiceLauncher();

  DappNetServiceLauncher(const DappNetServiceLauncher&) = delete;
  DappNetServiceLauncher& operator=(const DappNetServiceLauncher&) = delete;

  // Starts the DappNet service if not already running.
  // Returns true if service was started or already running.
  bool Start();

  // Stops the DappNet service if running.
  void Stop();

  // Checks if the service is currently running.
  bool IsRunning() const;

 private:
  // Task runner for background operations
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  
  // PID of the running service process, or 0 if not running
  mutable int service_pid_ = 0;
};

// Global instance accessor
DappNetServiceLauncher* GetDappNetServiceLauncher();

}  // namespace chrome

#endif  // CHROME_BROWSER_DAPPNET_DAPPNET_SERVICE_LAUNCHER_H_