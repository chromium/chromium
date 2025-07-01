#ifndef CHROME_BROWSER_ML_SERVER_UDS_MANAGER_H_
#define CHROME_BROWSER_ML_SERVER_UDS_MANAGER_H_

#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/feature_list.h"
#include "content/public/common/content_features.h"

class MLServerUdsManager {
 public:
  // Singleton instance getter
  static MLServerUdsManager& GetInstance();

  // Start and stop ML server
  void StartMLServerIfEnabled();
  void StopMLServer();

 private:
  // Private constructor to enforce singleton
  MLServerUdsManager();
  ~MLServerUdsManager();

  std::unique_ptr<base::Process> ml_server_uds_process_;

  // Prevent copying
  MLServerUdsManager(const MLServerUdsManager&) = delete;
  MLServerUdsManager& operator=(const MLServerUdsManager&) = delete;
};

#endif  // CHROME_BROWSER_ML_SERVER_UDS_MANAGER_H_
