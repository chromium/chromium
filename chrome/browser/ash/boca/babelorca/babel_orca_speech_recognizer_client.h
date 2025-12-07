// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BABELORCA_BABEL_ORCA_SPEECH_RECOGNIZER_CLIENT_H_
#define CHROME_BROWSER_ASH_BOCA_BABELORCA_BABEL_ORCA_SPEECH_RECOGNIZER_CLIENT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "chromeos/ash/components/boca/babelorca/soda_installer.h"
#include "media/mojo/mojom/speech_recognition.mojom-forward.h"

class Profile;
class SpeechRecognizer;

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace ash::babelorca {

class BabelOrcaSpeechRecognizerClient : public BabelOrcaSpeechRecognizer,
                                        public SpeechRecognizerDelegate {
 public:
  using SpeechRecognizerFactory =
      base::RepeatingCallback<std::unique_ptr<SpeechRecognizer>(
          const base::WeakPtr<SpeechRecognizerDelegate>&,
          Profile*,
          const std::string&,
          media::mojom::SpeechRecognitionOptionsPtr)>;

  explicit BabelOrcaSpeechRecognizerClient(
      Profile* profile,
      SodaInstaller* soda_installer,
      const std::string& application_locale,
      const std::string& caption_language);

  BabelOrcaSpeechRecognizerClient(const BabelOrcaSpeechRecognizerClient&) =
      delete;
  BabelOrcaSpeechRecognizerClient& operator=(
      const BabelOrcaSpeechRecognizerClient&) = delete;

  ~BabelOrcaSpeechRecognizerClient() override;

  // SpeechRecognizerDelegate:
  void OnSpeechResult(
      const std::u16string& text,
      bool is_final,
      const std::optional<media::SpeechRecognitionResult>& result) override;
  void OnSpeechSoundLevelChanged(int16_t level) override;
  void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) override;
  void OnSpeechRecognitionStopped() override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;

  // BabelOrcaSpeechRecognizer:
  void Start() override;
  void Stop() override;
  void AddObserver(Observer* obs) override;
  void RemoveObserver(Observer* obs) override;

  void SetSpeechRecognizerFactoryForTesting(
      SpeechRecognizerFactory speech_recognizer_factory);

 private:
  void OnSodaInstallResult(SodaInstaller::InstallationStatus status);

  SEQUENCE_CHECKER(sequence_checker_);
  // Indicates if the recognition was started or stopped by the user to avoid
  // starting recognition on soda installation success if it was stopped by the
  // user.
  bool started_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  // installer is owned by manager, which owns this class.
  raw_ptr<SodaInstaller> soda_installer_;
  raw_ptr<Profile> primary_profile_;
  const std::string default_language_;
  std::string source_language_;
  base::ObserverList<BabelOrcaSpeechRecognizer::Observer> observers_;
  SpeechRecognizerStatus speech_recognizer_status_ =
      SpeechRecognizerStatus::SPEECH_RECOGNIZER_OFF;
  std::unique_ptr<SpeechRecognizer> speech_recognizer_;
  SpeechRecognizerFactory speech_recognizer_factory_;
  base::WeakPtrFactory<BabelOrcaSpeechRecognizerClient> weak_ptr_factory_{this};
};

}  // namespace ash::babelorca

#endif  // CHROME_BROWSER_ASH_BOCA_BABELORCA_BABEL_ORCA_SPEECH_RECOGNIZER_CLIENT_H_
