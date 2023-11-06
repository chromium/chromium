// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/optical_character_recognizer.h"

#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "content/public/browser/browser_thread.h"

namespace app_list {

OpticalCharacterRecognizer::OpticalCharacterRecognizer() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

OpticalCharacterRecognizer::~OpticalCharacterRecognizer() = default;

bool OpticalCharacterRecognizer::IsServiceReady() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return (
      screen_ai::ScreenAIInstallState::GetInstance() &&
      screen_ai::ScreenAIInstallState::GetInstance()->IsComponentAvailable());
}

void OpticalCharacterRecognizer::EnsureAnnotatorIsConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (screen_ai_annotator_.is_bound()) {
    return;
  }

  DCHECK(IsServiceReady());
  screen_ai_service_router_.BindScreenAIAnnotator(
      screen_ai_annotator_.BindNewPipeAndPassReceiver());
  screen_ai_annotator_.reset_on_disconnect();
}

void OpticalCharacterRecognizer::InitializeComponent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsServiceReady()) {
    EnsureAnnotatorIsConnected();
  } else {
    // DLC downloader cannot run from current sequence.
    content::GetUIThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce([]() {
          // Screen AI Install State may be unavailable for tests.
          if (screen_ai::ScreenAIInstallState::GetInstance()) {
            screen_ai::ScreenAIInstallState::GetInstance()->DownloadComponent();
          }
        }));
  }
}

void OpticalCharacterRecognizer::DisconnectAnnotator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  screen_ai_annotator_.reset();
}

void OpticalCharacterRecognizer::ReadImage(
    const ::SkBitmap& image,
    base::OnceCallback<void(screen_ai::mojom::VisualAnnotationPtr)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureAnnotatorIsConnected();
  screen_ai_annotator_->PerformOcrAndReturnAnnotation(image,
                                                      std::move(callback));
}

}  // namespace app_list
