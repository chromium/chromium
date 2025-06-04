#ifndef CHROME_BROWSER_ML_SERVER_MANAGER_H_
#define CHROME_BROWSER_ML_SERVER_MANAGER_H_

#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/feature_list.h"
#include "content/public/common/content_features.h"

class MLServerManager {
 public:
  // Singleton instance getter
  static MLServerManager& GetInstance();

  // Start and stop ML server
  void StartMLServerIfEnabled();
  void StopMLServer();

 private:
  // Private constructor to enforce singleton
  MLServerManager();
  ~MLServerManager();

  std::unique_ptr<base::Process> ml_server_process_;

  // Prevent copying
  MLServerManager(const MLServerManager&) = delete;
  MLServerManager& operator=(const MLServerManager&) = delete;
};

#endif  // CHROME_BROWSER_ML_SERVER_MANAGER_H_
