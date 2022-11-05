// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/dictionary_event_router.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/input_method_api.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace OnDictionaryChanged =
    extensions::api::input_method_private::OnDictionaryChanged;
namespace OnDictionaryLoaded =
    extensions::api::input_method_private::OnDictionaryLoaded;

namespace chromeos {

ExtensionDictionaryEventRouter::ExtensionDictionaryEventRouter(
    content::BrowserContext* context)
    : context_(context), loaded_() {
  SpellcheckService* spellcheck = SpellcheckServiceFactory::GetForContext(
      context_);
  if (spellcheck) {
    service_ = spellcheck->GetWeakPtr();
    service_->GetCustomDictionary()->AddObserver(this);
    loaded_ = service_->GetCustomDictionary()->IsLoaded();
  }
}

ExtensionDictionaryEventRouter::~ExtensionDictionaryEventRouter() {
  if (service_)
    service_->GetCustomDictionary()->RemoveObserver(this);
}

void ExtensionDictionaryEventRouter::DispatchLoadedEventIfLoaded() {
  if (!loaded_)
    return;

  extensions::EventRouter* router = extensions::EventRouter::Get(context_);
  if (!router->HasEventListener(OnDictionaryLoaded::kEventName)) {
    return;
  }

  // The router will only send the event to extensions that are listening.
  auto event = std::make_unique<extensions::Event>(
      extensions::events::INPUT_METHOD_PRIVATE_ON_DICTIONARY_LOADED,
      OnDictionaryLoaded::kEventName, base::Value::List(), context_);
  router->BroadcastEvent(std::move(event));
}

void ExtensionDictionaryEventRouter::OnCustomDictionaryLoaded() {
  loaded_ = true;
  DispatchLoadedEventIfLoaded();
}

void ExtensionDictionaryEventRouter::OnCustomDictionaryChanged(
    const SpellcheckCustomDictionary::Change& dictionary_change) {
  extensions::EventRouter* router = extensions::EventRouter::Get(context_);

  if (!router->HasEventListener(OnDictionaryChanged::kEventName)) {
    return;
  }

  base::Value::List added_words;
  added_words.reserve(dictionary_change.to_add().size());
  for (const std::string& word : dictionary_change.to_add())
    added_words.Append(word);

  base::Value::List removed_words;
  removed_words.reserve(dictionary_change.to_remove().size());
  for (const std::string& word : dictionary_change.to_remove())
    removed_words.Append(word);

  base::Value::List args;
  args.Append(std::move(added_words));
  args.Append(std::move(removed_words));

  // The router will only send the event to extensions that are listening.
  auto event = std::make_unique<extensions::Event>(
      extensions::events::INPUT_METHOD_PRIVATE_ON_DICTIONARY_CHANGED,
      OnDictionaryChanged::kEventName, std::move(args), context_);
  router->BroadcastEvent(std::move(event));
}

}  // namespace chromeos
