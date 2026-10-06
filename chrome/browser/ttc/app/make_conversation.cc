// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/public/make_conversation.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ttc/app/audio_controller.h"
#include "chrome/browser/ttc/app/conversation_impl.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "components/ttc/app/ttc_mes_client.h"

namespace ttc {

std::unique_ptr<Conversation> MakeConversationImpl(
    SessionController& session_controller) {
  return std::make_unique<ConversationImpl>(
      std::make_unique<TtcMesClient>(
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              session_controller.GetProfile())),
      std::make_unique<AudioController>(
          AudioController::GetDefaultAudioStreamFactoryBinder()),
      session_controller);
}

std::unique_ptr<Conversation> MakeConversationImplForTesting(
    SessionController& session_controller,
    std::unique_ptr<AudioController> audio_controller) {
  CHECK(audio_controller);
  return std::make_unique<ConversationImpl>(
      std::make_unique<TtcMesClient>(
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              session_controller.GetProfile())),
      std::move(audio_controller), session_controller);
}

}  // namespace ttc
