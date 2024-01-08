// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_SPEECH_RECOGNITION_IMPL_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_SPEECH_RECOGNITION_IMPL_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_delegate.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/accessibility/public/mojom/assistive_technology_type.mojom.h"
#include "services/accessibility/public/mojom/speech_recognition.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class SpeechRecognitionPrivateRecognizer;
}  // namespace extensions

namespace ash {

// SpeechRecognitionImpl handles speech recognition requests from the
// Accessibility Service and also routes speech recognition events back to
// clients.
class SpeechRecognitionImpl
    : public ax::mojom::SpeechRecognition,
      public extensions::SpeechRecognitionPrivateDelegate {
 public:
  // Wraps a SpeechRecognitionEventObserver remote. The lifetime of this object
  // is a session of speech recognition e.g. it's created when speech
  // recognition starts and is destroyed when speech recognition stops.
  class SpeechRecognitionEventObserverWrapper {
   public:
    SpeechRecognitionEventObserverWrapper();
    ~SpeechRecognitionEventObserverWrapper();
    SpeechRecognitionEventObserverWrapper(
        const SpeechRecognitionEventObserverWrapper&) = delete;
    SpeechRecognitionEventObserverWrapper& operator=(
        const SpeechRecognitionEventObserverWrapper&) = delete;

    // Called whenever speech recognition stops.
    void OnStop();

    // Called when a speech recognition result is returned.
    void OnResult(ax::mojom::SpeechRecognitionResultEventPtr event);

    // Called when speech recognition encounters an error.
    void OnError(ax::mojom::SpeechRecognitionErrorEventPtr event);

    mojo::PendingReceiver<ax::mojom::SpeechRecognitionEventObserver>
    PassReceiver();

   private:
    mojo::Remote<ax::mojom::SpeechRecognitionEventObserver> observer_;
  };

  // Constructs a new SpeechRecognitionImpl for the given `profile`. This
  // SpeechRecognitionImpl will be reset when the profile changes so it can
  // assume that the profile is valid for its entire lifetime.
  explicit SpeechRecognitionImpl(content::BrowserContext* profile);
  SpeechRecognitionImpl(const SpeechRecognitionImpl&) = delete;
  SpeechRecognitionImpl& operator=(const SpeechRecognitionImpl&) = delete;
  ~SpeechRecognitionImpl() override;

  void Bind(mojo::PendingReceiver<ax::mojom::SpeechRecognition> receiver);

  // ax::mojom::SpeechRecognition:
  void Start(ax::mojom::StartOptionsPtr options,
             StartCallback callback) override;
  void Stop(ax::mojom::StopOptionsPtr options, StopCallback callback) override;

  // extensions::SpeechRecognitionPrivateDelegate:
  void HandleSpeechRecognitionStopped(const std::string& key) override;
  void HandleSpeechRecognitionResult(const std::string& key,
                                     const std::u16string& transcript,
                                     bool is_final) override;
  void HandleSpeechRecognitionError(const std::string& key,
                                    const std::string& error) override;

  base::WeakPtr<SpeechRecognitionImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class SpeechRecognitionImplTest;

  // Called after speech recognition starts. Performs final setup and creates
  // the object that is passed back to the caller.
  void StartHelper(StartCallback callback,
                   const std::string& key,
                   speech::SpeechRecognitionType type,
                   std::optional<std::string> error);

  // Called after speech recognition stops. Performs final clean up and notifies
  // the caller using the StopCallback.
  void StopHelper(StopCallback callback,
                  const std::string& key,
                  std::optional<std::string> error);

  // Creates a key given a client ID.
  std::string CreateKey(ax::mojom::AssistiveTechnologyType type);

  // Returns the speech recognizer associated with the key. Creates one if
  // none exists.
  extensions::SpeechRecognitionPrivateRecognizer* GetSpeechRecognizer(
      const std::string& key);

  // Creates an event observer wrapper associated with the key, if none already
  // exists.
  void CreateEventObserverWrapper(const std::string& key);

  // Returns the event observer wrapper associated with the key, if one exists.
  SpeechRecognitionEventObserverWrapper* GetEventObserverWrapper(
      const std::string& key);

  // Destroys the event observer wrapper associated with the key.
  void RemoveEventObserverWrapper(const std::string& key);

  std::map<std::string,
           std::unique_ptr<extensions::SpeechRecognitionPrivateRecognizer>>
      recognizers_;
  std::map<std::string, std::unique_ptr<SpeechRecognitionEventObserverWrapper>>
      event_observer_wrappers_;
  mojo::ReceiverSet<ax::mojom::SpeechRecognition> receivers_;
  raw_ptr<content::BrowserContext> profile_ = nullptr;
  base::WeakPtrFactory<SpeechRecognitionImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_SPEECH_RECOGNITION_IMPL_H_
