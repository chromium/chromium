// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/input_method_event_router.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace OnChanged = extensions::api::input_method_private::OnChanged;

namespace chromeos {

ExtensionInputMethodEventRouter::ExtensionInputMethodEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  input_method::InputMethodManager::Get()->AddObserver(this);
}

ExtensionInputMethodEventRouter::~ExtensionInputMethodEventRouter() {
  input_method::InputMethodManager::Get()->RemoveObserver(this);
}

void ExtensionInputMethodEventRouter::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  // If an event is recieved from a different profile, e.g. while switching
  // between multiple profiles, ignore it.
  if (!profile->IsSameProfile(Profile::FromBrowserContext(context_)))
    return;

  extensions::EventRouter* router = extensions::EventRouter::Get(context_);
  if (!router->HasEventListener(OnChanged::kEventName))
    return;

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->AppendString(
      manager->GetActiveIMEState()->GetCurrentInputMethod().id());

  // The router will only send the event to extensions that are listening.
  auto event = std::make_unique<extensions::Event>(
      extensions::events::INPUT_METHOD_PRIVATE_ON_CHANGED,
      OnChanged::kEventName, std::move(args), context_);
  router->BroadcastEvent(std::move(event));
}

}  // namespace chromeos
