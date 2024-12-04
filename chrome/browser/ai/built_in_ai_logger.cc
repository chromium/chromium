// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/built_in_ai_logger.h"

AiLogMessage::AiLogMessage(const char* file, int line)
    : opt_guide_message_builder_(
          optimization_guide_common::mojom::LogSource::BUILT_IN_AI,
          file,
          line,
          OptimizationGuideLogger::GetInstance()) {}
