// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"

#include <utility>

#include "base/no_destructor.h"
#include "content/public/browser/service_process_host.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

// static
AISemanticEmbedderServiceLauncher* AISemanticEmbedderServiceLauncher::Get() {
  static base::NoDestructor<AISemanticEmbedderServiceLauncher> instance;
  return instance.get();
}

void AISemanticEmbedderServiceLauncher::RecordSuccessfulUse() {
  crash_tracker_.ResetCrashCount();
}

void AISemanticEmbedderServiceLauncher::LaunchService(
    mojo::PendingReceiver<passage_embeddings::mojom::PassageEmbeddingsService>
        receiver) {
  content::ServiceProcessHost::Launch<
      passage_embeddings::mojom::PassageEmbeddingsService>(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName("AI Embeddings Service")  // Unique name for JS API!
          .Pass());
}

void AISemanticEmbedderServiceLauncher::OnServiceDisconnected(bool is_idle) {
  if (!is_idle) {
    crash_tracker_.RecordCrash();
  }
}

bool AISemanticEmbedderServiceLauncher::AllowedToLaunch() const {
  return !crash_tracker_.IsCrashLimitReached();
}

AISemanticEmbedderServiceLauncher::AISemanticEmbedderServiceLauncher() =
    default;

AISemanticEmbedderServiceLauncher::~AISemanticEmbedderServiceLauncher() =
    default;

bool AISemanticEmbedderServiceLauncher::CrashTracker::IsCrashLimitReached()
    const {
  return consecutive_crashes_ >= kMaxCrashes;
}

void AISemanticEmbedderServiceLauncher::CrashTracker::ResetCrashCount() {
  consecutive_crashes_ = 0;
}

void AISemanticEmbedderServiceLauncher::CrashTracker::RecordCrash() {
  consecutive_crashes_++;
}
