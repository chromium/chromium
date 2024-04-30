// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/chrome_passage_embeddings_service_controller.h"

#include "content/public/browser/service_process_host.h"

namespace history_embeddings {

// TODO(b/333094780): Figure out if we want to switch to using a global
// instance in an anonymous namespace.
// static
ChromePassageEmbeddingsServiceController*
ChromePassageEmbeddingsServiceController::Get() {
  static ChromePassageEmbeddingsServiceController* instance =
      new ChromePassageEmbeddingsServiceController();
  return instance;
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
