// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_KEYED_SERVICE_H_
#define CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_KEYED_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;
class GURL;
class LCPCriticalPathPredictorPersister;

namespace base {
class SequencedTaskRunner;
}

// KeyedService for LCP Critical Path Predictor.
//
// This class becomes ready asynchronously, so callers should check the
// state by calling the `IsReady()` method. For example:
//
// LCPCriticalPathPredictorKeyedService* predictor =
//     LCPCriticalPathPredictorKeyedServiceFactory::GetForProfile(profile);
// if (predictor && predictor->IsReady()) {
//   predictor->SetLCPElement(...);
// }
//
// If this class's methods are called when the class is not ready, a CHECK
// failure will occur.
class LCPCriticalPathPredictorKeyedService : public KeyedService {
 public:
  LCPCriticalPathPredictorKeyedService(
      Profile* profile,
      scoped_refptr<base::SequencedTaskRunner> db_task_runner);

  LCPCriticalPathPredictorKeyedService(
      const LCPCriticalPathPredictorKeyedService&) = delete;
  LCPCriticalPathPredictorKeyedService& operator=(
      const LCPCriticalPathPredictorKeyedService&) = delete;

  ~LCPCriticalPathPredictorKeyedService() override;

  bool IsReady() const;

  absl::optional<LCPElement> GetLCPElement(const GURL& page_url);
  void SetLCPElement(const GURL& page_url, const LCPElement& lcp_element);

 private:
  // KeyedService
  void Shutdown() override;

  void OnPersisterCreated(
      std::unique_ptr<LCPCriticalPathPredictorPersister> persister);

  // `persister_` can be nullptr since `persister_` is asynchronously set.
  std::unique_ptr<LCPCriticalPathPredictorPersister> persister_;

  base::WeakPtrFactory<LCPCriticalPathPredictorKeyedService> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_KEYED_SERVICE_H_
