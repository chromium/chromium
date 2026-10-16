// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/public/make_conversation.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ttc/app/audio_controller_impl.h"
#include "chrome/browser/ttc/app/conversation_impl.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "components/ttc/app/public/session_journal.h"
#include "components/ttc/app/ttc_mes_client.h"

namespace ttc {

std::unique_ptr<Conversation> MakeConversationImpl(
    SessionController& session_controller,
    std::unique_ptr<AudioController> audio_controller) {
  if (!audio_controller) {
    audio_controller = std::make_unique<AudioControllerImpl>(
        AudioControllerImpl::GetDefaultAudioStreamFactoryBinder());
  }
  return std::make_unique<ConversationImpl>(
      std::make_unique<TtcMesClient>(
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              session_controller.GetProfile()),
          &session_controller.GetJournal()),
      std::move(audio_controller), session_controller);
}

}  // namespace ttc
