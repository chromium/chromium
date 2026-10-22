// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_HARNESS_PAGE_HANDLER_H_
#define CHROME_BROWSER_TTC_CORE_HARNESS_PAGE_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ttc/core/eval_harness.mojom.h"
#include "chrome/browser/ttc/core/ttc_eval_harness.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ttc {

class TtcKeyedService;

// Coordinates communication between browser C++ and WebUI test runner
// (eval_app.ts) via eval_harness.mojom.
class HarnessPageHandler : public mojom::HarnessPageHandler {
 public:
  HarnessPageHandler(TtcKeyedService& service,
                     mojo::PendingReceiver<mojom::HarnessPageHandler> receiver);
  ~HarnessPageHandler() override;
  HarnessPageHandler(const HarnessPageHandler&) = delete;
  HarnessPageHandler& operator=(const HarnessPageHandler&) = delete;

  // mojom::HarnessPageHandler implementation:
  void StartSession(StartSessionCallback callback) override;
  void CloseSession() override;
  void SendText(const std::string& text, SendTextCallback callback) override;
  void SendAudio(const std::vector<uint8_t>& pcm16_bytes,
                 SendAudioCallback callback) override;

  TtcEvalHarness* eval_harness() { return eval_harness_.get(); }

 private:
  const raw_ptr<TtcKeyedService> service_;
  mojo::Receiver<mojom::HarnessPageHandler> receiver_;

  std::unique_ptr<TtcEvalHarness> eval_harness_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_HARNESS_PAGE_HANDLER_H_
