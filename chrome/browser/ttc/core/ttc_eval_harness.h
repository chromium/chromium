// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TTC_EVAL_HARNESS_H_
#define CHROME_BROWSER_TTC_CORE_TTC_EVAL_HARNESS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ttc/core/eval_harness.mojom.h"

namespace ttc {

class TtcKeyedService;

// Test and evaluation harness connector for driving TTC sessions.
// Connects HarnessPageHandler to session control and automated turn execution.
class TtcEvalHarness {
 public:
  using TurnCompleteCallback = base::OnceCallback<void(mojom::TurnResultPtr)>;
  using StartSessionCallback = base::OnceCallback<void(bool success)>;

  explicit TtcEvalHarness(TtcKeyedService& service);
  virtual ~TtcEvalHarness();
  TtcEvalHarness(const TtcEvalHarness&) = delete;
  TtcEvalHarness& operator=(const TtcEvalHarness&) = delete;

  // Starts a test session. Resolves once the session is initialized and ready.
  virtual void StartSession(StartSessionCallback callback);

  // Ends the active test session.
  virtual void EndSession();

  // Executes an automated text turn.
  virtual void SendText(const std::string& text, TurnCompleteCallback callback);

  // Executes an automated audio turn with raw 16kHz 16-bit mono PCM bytes.
  virtual void SendAudio(const std::vector<uint8_t>& pcm16_bytes,
                         TurnCompleteCallback callback);

 private:
  const raw_ref<TtcKeyedService> service_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TTC_EVAL_HARNESS_H_
