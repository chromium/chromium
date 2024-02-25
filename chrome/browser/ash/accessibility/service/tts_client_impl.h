// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_TTS_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_TTS_CLIENT_IMPL_H_

#include "chrome/browser/profiles/profile.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/accessibility/public/mojom/tts.mojom.h"

namespace ash {

// The TtsClientImpl receives text-to-speech requests from the Accessibility
// Service and sends back information about the text-to-speech state.
class TtsClientImpl : public ax::mojom::Tts {
 public:
  // Constructs a new TtsClientImpl for the given `profile`. This TtsClientImpl
  // will be reset when the profile changes so it can assume that the profile
  // is valid for its entire lifetime.
  explicit TtsClientImpl(content::BrowserContext* profile);
  TtsClientImpl(const TtsClientImpl&) = delete;
  TtsClientImpl& operator=(const TtsClientImpl&) = delete;
  ~TtsClientImpl() override;

  void Bind(mojo::PendingReceiver<Tts> tts_receiver);

  // ax::mojom::Tts:
  void Speak(const std::string& utterance,
             ax::mojom::TtsOptionsPtr options,
             SpeakCallback callback) override;
  void Stop() override;
  void Pause() override;
  void Resume() override;
  void IsSpeaking(IsSpeakingCallback callback) override;
  void GetVoices(GetVoicesCallback callback) override;

  // TODO(b/323189746): Add onVoicesChanged listener in ATP.

 private:
  mojo::ReceiverSet<ax::mojom::Tts> tts_receivers_;
  raw_ptr<content::BrowserContext> profile_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_TTS_CLIENT_IMPL_H_
