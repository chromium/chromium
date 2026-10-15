// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TTC_EVAL_HARNESS_H_
#define CHROME_BROWSER_TTC_CORE_TTC_EVAL_HARNESS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ttc/core/eval_audio_controller.h"
#include "chrome/browser/ttc/core/eval_harness.mojom.h"
#include "chrome/browser/ttc/core/session_controller.h"

namespace ttc {

class TtcKeyedService;

// Test and evaluation harness for driving TTC sessions end-to-end.
// Orchestrates automated turns, injecting canned audio or text inputs,
// observing turn lifecycle and tool execution via SessionController::Observer,
// and returning the consolidated mojom::TurnResult.
class TtcEvalHarness : public SessionController::Observer {
 public:
  using TurnCompleteCallback = base::OnceCallback<void(mojom::TurnResultPtr)>;
  using StartSessionCallback = base::OnceCallback<void(bool success)>;

  explicit TtcEvalHarness(TtcKeyedService& service);
  ~TtcEvalHarness() override;
  TtcEvalHarness(const TtcEvalHarness&) = delete;
  TtcEvalHarness& operator=(const TtcEvalHarness&) = delete;

  // Starts a test session with an EvalAudioController injected.
  // Resolves once the session lifecycle reaches kLive.
  void StartSession(StartSessionCallback callback);

  // Ends the active test session.
  void EndSession();

  // Executes an automated text turn.
  void SendText(const std::string& text, TurnCompleteCallback callback);

  // Executes an automated audio turn with 16kHz 16-bit mono PCM samples.
  void SendAudio(base::span<const int16_t> pcm16_samples,
                 TurnCompleteCallback callback);

  // SessionController::Observer implementation:
  void OnSessionLifecycleChanged(SessionLifecycle state) override;
  void OnModelTurnComplete() override;
  void OnModelInterrupted() override;
  void OnError(ErrorCode error) override;
  void OnToolExecutionCompleted(const std::string& tool_name,
                                bool success) override;
  void OnInputTranscription(const std::string& text) override;
  void OnOutputTranscription(const std::string& text) override;

  EvalAudioController* audio_controller() { return audio_controller_.get(); }

 private:
  void BeginUserTurn(TurnCompleteCallback callback);
  void PumpAudio();
  void StopAudioInjection();
  void FinishTurn();
  void OnTurnTimeout();
  void OnStartSessionTimeout();

  const raw_ref<TtcKeyedService> service_;
  base::WeakPtr<EvalAudioController> audio_controller_;
  base::ScopedObservation<SessionController, SessionController::Observer>
      observation_{this};

  StartSessionCallback start_session_callback_;
  base::OneShotTimer start_session_timeout_;

  TurnCompleteCallback turn_complete_callback_;
  mojom::TurnResultPtr current_turn_result_;
  base::OneShotTimer turn_timeout_;

  // Canned audio pump state.
  std::vector<int16_t> injected_audio_;
  size_t injected_audio_offset_ = 0;
  base::RepeatingTimer audio_pump_timer_;

  base::WeakPtrFactory<TtcEvalHarness> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TTC_EVAL_HARNESS_H_
