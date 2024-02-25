// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_ASH_TRACING_HANDLER_H_
#define ASH_HUD_DISPLAY_ASH_TRACING_HANDLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/files/platform_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/trace_event/trace_config.h"

namespace perfetto {
class TracingSession;
}

namespace ash {
namespace hud_display {

class AshTracingRequest;

// Only one instance of this object can exist at a time.
class ASH_EXPORT AshTracingHandler {
 public:
  AshTracingHandler();
  AshTracingHandler(const AshTracingHandler&) = delete;
  AshTracingHandler& operator=(const AshTracingHandler&) = delete;
  ~AshTracingHandler();

  // Initiates tracing start. Observer will be notified with the result.
  void Start(AshTracingRequest* request);

  // Initiates tracing stop. Observer will be notified with the result.
  void Stop();

  // Returns true if tracing was started.
  bool IsStarted() const;

  // This allows to use fake Perfetto sessions for testing.
  static void SetPerfettoTracingSessionCreatorForTesting(
      std::unique_ptr<perfetto::TracingSession> (*creator)(void));
  static void ResetPerfettoTracingSessionCreatorForTesting();

 private:
  void OnTracingStarted();
  void OnTracingFinished();

  raw_ptr<AshTracingRequest> request_ = nullptr;

  std::unique_ptr<perfetto::TracingSession> tracing_session_;

  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<AshTracingHandler> weak_factory_{this};
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_ASH_TRACING_HANDLER_H_
