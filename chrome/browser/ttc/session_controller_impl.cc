// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/session_controller_impl.h"

#include "chrome/browser/ttc/conversation.h"
#include "chrome/browser/ttc/session_view.h"
#include "chrome/browser/ttc/ttc_keyed_service.h"

namespace ttc {

SessionControllerImpl::SessionControllerImpl(TtcKeyedService& service)
    : service_(service),
      conversation_(Conversation::Create(service.profile())),
      session_view_(std::make_unique<SessionView>(*this)) {}

SessionControllerImpl::~SessionControllerImpl() = default;

Conversation* SessionControllerImpl::GetConversation() {
  return conversation_.get();
}

}  // namespace ttc
