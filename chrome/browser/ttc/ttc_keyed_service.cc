// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/ttc_keyed_service.h"

#include <utility>

#include "chrome/browser/ttc/app/public/make_conversation.h"
#include "chrome/browser/ttc/conversation.h"
#include "chrome/browser/ttc/session_controller_impl.h"
#include "chrome/browser/ttc/ttc_keyed_service_factory.h"

namespace ttc {

// static
TtcKeyedService* TtcKeyedService::Get(content::BrowserContext* context) {
  return TtcKeyedServiceFactory::GetTtcKeyedService(context);
}

TtcKeyedService::TtcKeyedService(Profile* profile,
                                 ConversationFactory conversation_factory)
    : profile_(profile),
      conversation_factory_(std::move(conversation_factory)) {}

TtcKeyedService::~TtcKeyedService() = default;

void TtcKeyedService::Shutdown() {
  EndSession();
}

void TtcKeyedService::StartSession() {
  CHECK(!session_controller_);
  session_controller_ = std::make_unique<SessionControllerImpl>(*this);
}

void TtcKeyedService::EndSession() {
  session_controller_.reset();
}

std::unique_ptr<Conversation> TtcKeyedService::MakeConversation(
    base::PassKey<SessionControllerImpl>) {
  std::unique_ptr<Conversation> conversation =
      conversation_factory_ ? conversation_factory_.Run(profile_)
                            : MakeConversationImpl(profile_);
  CHECK(conversation);
  return conversation;
}

}  // namespace ttc
