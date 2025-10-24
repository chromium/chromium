// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_PUBLIC_OPTICAL_CHARACTER_RECOGNIZER_H_
#define CHROME_BROWSER_SCREEN_AI_PUBLIC_OPTICAL_CHARACTER_RECOGNIZER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
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
// See OCR section in `chrome/browser/screen_ai/README.md` for more info.
class OpticalCharacterRecognizer
    : public ProfileObserver,
      public base::RefCountedDeleteOnSequence<OpticalCharacterRecognizer> {
 public:
  using OcrDisconnectedCallback = base::RepeatingCallback<void()>;

  // Creates OCR using ScreenAI service instance for `profile`. If needed,
  // triggers download and initialization of the component. Calls
  // `status_callback` asynchronously after the OCR has been created and the
  // service initialization status is known.
  static scoped_refptr<screen_ai::OpticalCharacterRecognizer>
  CreateWithStatusCallback(Profile* profile,
                           mojom::OcrClientType client_type,
                           base::OnceCallback<void(bool)> status_callback);

  // Creates OCR using ScreenAI service instance for `profile`. If needed,
  // triggers download and initialization of the component.
  static scoped_refptr<screen_ai::OpticalCharacterRecognizer> Create(
      Profile* profile,
      mojom::OcrClientType client_type);

  // Creates OCR for testing. The object will not be connected to ScreenAI
  // service and always returns empty results.
  static scoped_refptr<screen_ai::OpticalCharacterRecognizer>
  CreateForTesting();

  OpticalCharacterRecognizer(const OpticalCharacterRecognizer&) = delete;
  OpticalCharacterRecognizer& operator=(const OpticalCharacterRecognizer&) =
      delete;

  // ProfileObserver::
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Returns true if OCR service is ready. This state will be preserved if the
  // connection to the OCR service is reset due to being idle or if the service
  // is shut down. It is expected that the connection would be revivable when
  // needed.
  bool is_ready() { return ready_ && *ready_; }

  bool is_connected() {
    return screen_ai_annotator_ && screen_ai_annotator_->is_bound() &&
           screen_ai_annotator_->is_connected();
  }

  bool StatusAvailableForTesting() { return ready_.has_value(); }

  // Connects to the OCR service if not already connected.
  void MaybeConnectToOcrService();

  // Performs OCR on the given image and returns the results as a
  // `VisualAnnotation` struct. Returns empty results in the callback if the
  // service is not ready yet.
  virtual void PerformOCR(
      const SkBitmap& image,
      base::OnceCallback<void(mojom::VisualAnnotationPtr)> callback);

#if BUILDFLAG(IS_CHROMEOS)
  // Performs OCR on the given image and returns the results as an accessibility
  // tree update. Returns empty results in the callback if the service is not
  // ready yet.
  virtual void PerformOCR(
      const SkBitmap& image,
      base::OnceCallback<void(const ui::AXTreeUpdate& tree_update)> callback);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Sets the OCR to light or normal mode.
  virtual void SetOCRLightMode(bool enabled);

  // Tells if OCR is busy (has another connected client) or not.
  virtual void IsOCRBusy(mojom::ScreenAIAnnotator::IsOCRBusyCallback callback);

  // Ensures all posted tasks are completed in tests.
  virtual void FlushForTesting() {}

  // Disconnects from ScreenAI service. This can also be used to simulate idle
  // timeout or service shutdown/crash for testing.
  void DisconnectAnnotator();

  // Sets the callback for the disconnection of `screen_ai_annotator_`. It will
  // be triggered from the UI thread, and it's the responsibility of the client
  // to execute it on the right thread.
  void SetDisconnectedCallback(OcrDisconnectedCallback callback);

  // Returns the maximum dimension for which images are processed without
  // downsampling. This value is not expected to change after initialization of
  // the service and is expected to be non-zero. Returns 0 in the callback if
  // the service is not ready yet.
  void GetMaxImageDimension(base::OnceCallback<void(uint32_t)> callback);

 protected:
  explicit OpticalCharacterRecognizer(Profile* profile,
                                      mojom::OcrClientType client_type);
  ~OpticalCharacterRecognizer() override;

  // OCR Service is ready to use. The value is set after initialization has
  // finished successfully or with failure.
  std::optional<bool> ready_;

 private:
  friend class base::RefCountedDeleteOnSequence<OpticalCharacterRecognizer>;
  friend class base::DeleteHelper<OpticalCharacterRecognizer>;
  template <typename T, typename... Args>
  friend scoped_refptr<T> base::MakeRefCounted(Args&&... args);

  void Initialize(base::OnceCallback<void(bool)> status_callback);

  // `status_callback` will receive a copy of `successful`.
  void OnOCRInitializationCallback(
      base::OnceCallback<void(bool)> status_callback,
      bool successful);

  void OnOcrDisconnected();

  // Is initialized in the constructor and is cleared if profile gets destroyed
  // while this object still exists.
  raw_ptr<Profile> profile_;

  mojom::OcrClientType client_type_;

  // Called when the `screen_ai_annotator_` is disconnected.
  OcrDisconnectedCallback ocr_disconnected_callback_;

  // For calls from another sequence, this object keeps a pointer to the task
  // scheduler of the other sequence to return the result.
  // Each `OpticalCharacterRecognizer` object can be used for at most one other
  // sequence.
  std::unique_ptr<base::SequenceBound<SequenceBoundReceiver>>
      sequence_bound_receiver_;

  std::unique_ptr<mojo::Remote<mojom::ScreenAIAnnotator>> screen_ai_annotator_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observer_{this};

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<OpticalCharacterRecognizer> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_PUBLIC_OPTICAL_CHARACTER_RECOGNIZER_H_
