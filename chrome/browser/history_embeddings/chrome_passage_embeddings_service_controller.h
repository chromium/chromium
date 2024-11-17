// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
#define CHROME_BROWSER_HISTORY_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "components/history_embeddings/passage_embeddings_service_controller.h"

namespace base {

class Process;

}  // namespace base

namespace history_embeddings {

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

  static ChromePassageEmbeddingsServiceController** GetInstanceStorage();

  // PassageEmbeddingsServiceController implementation:
  void LaunchService() override;

  // Initializes `cpu_logger_`; can only be called when the service process is
  // launched and connected.
  void InitializeCpuLogger();

  // Called after service is launched.
  void OnServiceLaunched(const base::Process& process);

  // Used to generate weak pointers to self.
  base::WeakPtrFactory<ChromePassageEmbeddingsServiceController>
      weak_ptr_factory_{this};
};

}  // namespace history_embeddings

#endif  // CHROME_BROWSER_HISTORY_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
