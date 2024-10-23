// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BABELORCA_BABEL_ORCA_SPEECH_RECOGNIZER_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_BABELORCA_BABEL_ORCA_SPEECH_RECOGNIZER_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {

class BabelOrcaSpeechRecognizerImpl : public BabelOrcaSpeechRecognizer,
                                      public ash::SystemLiveCaptionService {
 public:
  explicit BabelOrcaSpeechRecognizerImpl(Profile* profile);
  ~BabelOrcaSpeechRecognizerImpl() override;

  // SystemLiveCaptionService
  void OnSpeechResult(
      const std::u16string& text,
      bool is_final,
      const std::optional<media::SpeechRecognitionResult>& result) override;

  // BabelOrcaSpeechRecognizer
  void Start() override;
  void Stop() override;
  void ObserveTranscriptionResult(
      BabelOrcaSpeechRecognizer::TranscriptionResultCallback
          transcription_result_callback) override;
  void RemoveTranscriptionResultObservation() override;

 private:
  raw_ptr<Profile> primary_profile_;
  BabelOrcaSpeechRecognizer::TranscriptionResultCallback
      transcription_result_callback_;
};

}  // namespace ash::babelorca

#endif  // CHROME_BROWSER_ASH_BOCA_BABELORCA_BABEL_ORCA_SPEECH_RECOGNIZER_IMPL_H_
