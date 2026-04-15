// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_message_handler.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"

namespace {

glic::GlicInvokeOptions CreateInvokeOptions(
    const components_sharing_message::GlicExperimentalTriggering& request,
    tabs::TabInterface* tab) {
  glic::GlicInvokeOptions options{glic::mojom::InvocationSource::kUnsupported};
  options.target.surface = tab;

  if (request.has_request() &&
      request.request().has_trigger_actuation_request() &&
      request.request().trigger_actuation_request().has_initial_prompt()) {
    options.prompts.push_back(
        request.request().trigger_actuation_request().initial_prompt());
  }

  if (request.has_task_metadata() &&
      request.task_metadata().has_conversation_id() &&
      !request.task_metadata().conversation_id().empty()) {
    options.target.conversation =
        glic::ConversationId(request.task_metadata().conversation_id());
  } else {
    options.target.conversation = glic::NewConversation();
  }

  return options;
}

}  // namespace

GlicExperimentalTriggeringMessageHandler::
    GlicExperimentalTriggeringMessageHandler(Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}

GlicExperimentalTriggeringMessageHandler::
    ~GlicExperimentalTriggeringMessageHandler() = default;

void GlicExperimentalTriggeringMessageHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicExperimentalTriggering));
  CHECK(message.has_glic_experimental_triggering());

  const auto& request = message.glic_experimental_triggering();

  BrowserWindowInterface* browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&browser, this](BrowserWindowInterface* b) {
        if (b->GetProfile() == profile_) {
          browser = b;
          return false;  // Stop iteration
        }
        return true;  // Continue
      });

  if (!browser) {
    DLOG(ERROR) << "No active browser window found for Profile for "
                   "GlicExperimentalTriggering";
    std::move(done_callback).Run(nullptr);
    return;
  }

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_,
                                                         /*create=*/false);
  CHECK(glic_service);

  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      CreateInvokeOptions(request, browser->GetActiveTabInterface()));

  std::move(done_callback).Run(nullptr);
}
