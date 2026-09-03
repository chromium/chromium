// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/glic_linkout_handler.h"

#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/glic_enums.mojom.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace critical_actions {

// static
GlicLinkoutHandler* GlicLinkoutHandler::GetInstance() {
  static base::NoDestructor<GlicLinkoutHandler> instance;
  return instance.get();
}

GlicLinkoutHandler::GlicLinkoutHandler() = default;
GlicLinkoutHandler::~GlicLinkoutHandler() = default;

void GlicLinkoutHandler::OpenConversation(
    content::WebContents* web_contents,
    const CriticalActionEntry& entry,
    glic::mojom::InvocationSource source,
    base::OnceCallback<void(OpenConversationResult)> callback) {
  if (entry.conversation_id.empty()) {
    std::move(callback).Run(OpenConversationResult::kErrorInvalidActionEntry);
    return;
  }

  if (!web_contents) {
    std::move(callback).Run(OpenConversationResult::kErrorInternal);
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  glic::GlicKeyedService* glic_service =
      profile ? glic::GlicKeyedService::Get(profile) : nullptr;

  if (profile && glic::GlicEnabling::IsEnabledForProfile(profile) &&
      glic_service) {
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_contents);
    if (tab) {
      glic::Target target(*tab);
      target.conversation = glic::ConversationId(entry.conversation_id);

      glic::GlicInvokeOptions options(std::move(target), source);
      options.fre_completion_wait_mode = glic::FreCompletionWaitMode::kNever;
      options.supersede_if_in_progress = true;
      options.pin_on_bind = false;

      glic_service->Invoke(std::move(options));
      std::move(callback).Run(OpenConversationResult::kSuccess);
      return;
    }
  }

  std::move(callback).Run(OpenConversationResult::kErrorInternal);
}

}  // namespace critical_actions
