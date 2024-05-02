// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/chrome_passage_embeddings_service_controller.h"

#include "content/public/browser/service_process_host.h"

namespace history_embeddings {

// static
ChromePassageEmbeddingsServiceController*
ChromePassageEmbeddingsServiceController::Get() {
  static base::NoDestructor<ChromePassageEmbeddingsServiceController> instance;
  return instance.get();
}

ChromePassageEmbeddingsServiceController::
    ChromePassageEmbeddingsServiceController() = default;

ChromePassageEmbeddingsServiceController::
    ~ChromePassageEmbeddingsServiceController() = default;

void ChromePassageEmbeddingsServiceController::LaunchService() {
  // No-op if already launched.
  if (service_remote_) {
    return;
  }

  auto receiver = service_remote_.BindNewPipeAndPassReceiver();
  service_remote_.reset_on_disconnect();
  service_remote_.set_idle_handler(
      base::Minutes(1),
      base::BindRepeating(
          &ChromePassageEmbeddingsServiceController::ResetRemotes,
          base::Unretained(this)));
  content::ServiceProcessHost::Launch<
      passage_embeddings::mojom::PassageEmbeddingsService>(
      std::move(receiver), content::ServiceProcessHost::Options()
                               .WithDisplayName("Passage Embeddings Service")
                               .Pass());
}

}  // namespace history_embeddings
