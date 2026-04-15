// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_handler.h"

class Profile;

class GlicExperimentalTriggeringMessageHandler : public SharingMessageHandler {
 public:
  explicit GlicExperimentalTriggeringMessageHandler(Profile* profile);
  GlicExperimentalTriggeringMessageHandler(
      const GlicExperimentalTriggeringMessageHandler&) = delete;
  GlicExperimentalTriggeringMessageHandler& operator=(
      const GlicExperimentalTriggeringMessageHandler&) = delete;
  ~GlicExperimentalTriggeringMessageHandler() override;

  void OnMessage(components_sharing_message::SharingMessage message,
                 DoneCallback done_callback) override;

 private:
  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_
