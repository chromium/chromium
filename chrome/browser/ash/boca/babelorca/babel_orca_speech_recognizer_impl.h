// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BABELORCA_BABEL_ORCA_SPEECH_RECOGNIZER_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_BABELORCA_BABEL_ORCA_SPEECH_RECOGNIZER_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "chromeos/ash/components/boca/babelorca/soda_installer.h"
#include "chromeos/ash/components/boca/babelorca/speech_recognition_event_handler.h"
#include "components/prefs/pref_service.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {

class BabelOrcaSpeechRecognizerImpl : public BabelOrcaSpeechRecognizer,
                                      public ash::SystemLiveCaptionService {
 public:
  explicit BabelOrcaSpeechRecognizerImpl(Profile* profile,
                                         SodaInstaller* soda_installer,
                                         const std::string& application_locale,
                                         const std::string& caption_language);

  BabelOrcaSpeechRecognizerImpl(const BabelOrcaSpeechRecognizerImpl&) = delete;
  BabelOrcaSpeechRecognizerImpl& operator=(
      const BabelOrcaSpeechRecognizerImpl&) = delete;

  ~BabelOrcaSpeechRecognizerImpl() override;

  // SystemLiveCaptionService
  void OnSpeechResult(
      const std::u16string& text,
      bool is_final,
      const std::optional<media::SpeechRecognitionResult>& result) override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;

  // BabelOrcaSpeechRecognizer
  void Start() override;
  void Stop() override;
  void AddObserver(Observer* obs) override;
  void RemoveObserver(Observer* obs) override;

 protected:
  media::mojom::RecognizerClientType GetRecognizerClientType() override;

 private:
  // SystemLiveCaptionService:
  std::string GetPrimaryLanguageCode() const override;

  void OnSpeechRecognitionAvailabilityChanged(
      SodaInstaller::InstallationStatus status);

  bool started_ = false;
  // installer is owned by manager, which owns this class.
  raw_ptr<SodaInstaller> soda_installer_;
  SpeechRecognitionEventHandler speech_recognition_event_handler_;
  raw_ptr<Profile> primary_profile_;
  std::string caption_language_;
  base::WeakPtrFactory<BabelOrcaSpeechRecognizerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::babelorca

#endif  // CHROME_BROWSER_ASH_BOCA_BABELORCA_BABEL_ORCA_SPEECH_RECOGNIZER_IMPL_H_
