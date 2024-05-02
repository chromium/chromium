// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
#define CHROME_BROWSER_HISTORY_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_

#include "base/no_destructor.h"
#include "components/history_embeddings/passage_embeddings_service_controller.h"

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
};

}  // namespace history_embeddings

#endif  // CHROME_BROWSER_HISTORY_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
