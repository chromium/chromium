// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_OPTICAL_CHARACTER_RECOGNIZER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_OPTICAL_CHARACTER_RECOGNIZER_H_

#include "base/sequence_checker.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"

namespace app_list {

class OpticalCharacterRecognizer {
 public:
  OpticalCharacterRecognizer();
  ~OpticalCharacterRecognizer();
  OpticalCharacterRecognizer(const OpticalCharacterRecognizer&) = delete;
  OpticalCharacterRecognizer& operator=(const OpticalCharacterRecognizer&) =
      delete;

  void EnsureAnnotatorIsConnected();
  void DisconnectAnnotator();
  void ReadImage(
      const ::SkBitmap& image,
      base::OnceCallback<void(screen_ai::mojom::VisualAnnotationPtr)> callback);
  bool IsServiceReady() const;
  void InitializeComponent();

 private:
  mojo::Remote<screen_ai::mojom::ScreenAIAnnotator> screen_ai_annotator_;
  // Controls the OCR library.
  screen_ai::ScreenAIServiceRouter screen_ai_service_router_;

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_OPTICAL_CHARACTER_RECOGNIZER_H_
