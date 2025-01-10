// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/passage_embeddings/cpu_histogram_logger.h"
#include "components/passage_embeddings/passage_embeddings_service_controller.h"

namespace base {
class Process;
}  // namespace base

namespace passage_embeddings {

// Chrome uses a single instance of PassageEmbeddingsServiceController. We only
// want to load the model once, not once per Profile. To do otherwise would
// consume a significant amount of memory.
class ChromePassageEmbeddingsServiceController
    : public PassageEmbeddingsServiceController {
 public:
  // Returns the PassageEmbeddingsServiceController.
  static ChromePassageEmbeddingsServiceController* Get();

 private:
  friend base::NoDestructor<ChromePassageEmbeddingsServiceController>;

  ChromePassageEmbeddingsServiceController();
  ~ChromePassageEmbeddingsServiceController() override;

  // PassageEmbeddingsServiceController implementation:
  void MaybeLaunchService() override;
  void ResetServiceRemote() override;

  // Initializes `cpu_logger_`; can only be called when the service process is
  // launched and connected.
  void InitializeCpuLogger();

  // Called after service is launched.
  void OnServiceLaunched(const base::Process& process);

  // When the embeddings service is running, the logger will periodically sample
  // and log the CPU time used by the service process.
  CpuHistogramLogger cpu_logger_;

  // Used to generate weak pointers to self.
  base::WeakPtrFactory<ChromePassageEmbeddingsServiceController>
      weak_ptr_factory_{this};
};

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
