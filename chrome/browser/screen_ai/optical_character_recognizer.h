// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_OPTICAL_CHARACTER_RECOGNIZER_H_
#define CHROME_BROWSER_SCREEN_AI_OPTICAL_CHARACTER_RECOGNIZER_H_

#include <optional>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace {
class SequenceBoundReceiver;
}

namespace screen_ai {

// A simple class to initialize and perform OCR service.
// See how to use OCR section in `/services/screen_ai/README.md` for more info.
class OpticalCharacterRecognizer
    : public ProfileObserver,
      public base::RefCountedDeleteOnSequence<OpticalCharacterRecognizer> {
 public:
  // Creates OCR using ScreenAI service instance for `profile`. If needed,
  // triggers download and initialization of the component.
  // TODO(crbug.com/327181467): Try add a constructor that receives a callback
  // when initialization is completed.
  explicit OpticalCharacterRecognizer(Profile* profile);

  OpticalCharacterRecognizer(const OpticalCharacterRecognizer&) = delete;
  OpticalCharacterRecognizer& operator=(const OpticalCharacterRecognizer&) =
      delete;

  // ProfileObserver::
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Returns true if OCR service is ready.
  bool is_ready() { return ready_ && *ready_; }

  // Performs OCR on the given image and returns the results. If the client is
  // not in the browser process, it needs to implement this function in its
  // own process.
  void PerformOCR(
      const SkBitmap& image,
      base::OnceCallback<void(mojom::VisualAnnotationPtr)> callback);

  // TODO(crbug.com/327181467): Add more infterfaces for a11y tree OCR output.

 private:
  friend class base::RefCountedDeleteOnSequence<OpticalCharacterRecognizer>;
  friend class base::DeleteHelper<OpticalCharacterRecognizer>;

  ~OpticalCharacterRecognizer() override;

  void OnOCRInitializationCallback(bool successful);

  // Is initialized in the constructor and is cleared if profile gets destroyed
  // while this object still exists, or after it is used in
  // `OnOCRInitializationCallback`.
  raw_ptr<Profile> profile_;

  // For calls from another sequence, this object keeps a pointer to the task
  // scheduler of the other sequence to return the result.
  // Each `OpticalCharacterRecognizer` object can be used for at most one other
  // sequence.
  std::unique_ptr<base::SequenceBound<SequenceBoundReceiver>>
      sequence_bound_receiver_;

  // OCR Service is ready to use.
  std::optional<bool> ready_;

  std::unique_ptr<mojo::Remote<mojom::ScreenAIAnnotator>> screen_ai_annotator_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observer_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_OPTICAL_CHARACTER_RECOGNIZER_H_
