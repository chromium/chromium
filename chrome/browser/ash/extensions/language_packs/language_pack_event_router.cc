// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/language_packs/language_pack_event_router.h"

#include <memory>

#include "chrome/browser/ash/extensions/language_packs/language_packs_extensions_util.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "extensions/browser/event_router.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace chromeos {

namespace input_method_private = ::extensions::api::input_method_private;
namespace language_packs = ::ash::language_packs;
namespace input_method = ::ash::input_method;
namespace OnLanguagePackStatusChanged =
    ::extensions::api::input_method_private::OnLanguagePackStatusChanged;

LanguagePackEventRouter::LanguagePackEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  obs_.Observe(language_packs::LanguagePackManager::GetInstance());
}

LanguagePackEventRouter::~LanguagePackEventRouter() {}

void LanguagePackEventRouter::OnPackStateChanged(
    const language_packs::PackResult& pack_result) {
  extensions::EventRouter* router = extensions::EventRouter::Get(context_);
  if (!router->HasEventListener(OnLanguagePackStatusChanged::kEventName)) {
    return;
  }

  if (pack_result.feature_id != language_packs::kHandwritingFeatureId) {
    return;
  }

  input_method_private::LanguagePackStatusChange change;
  change.engine_ids =
      input_method::InputMethodManager::Get()
          ->GetInputMethodUtil()
          ->GetInputMethodIdsFromHandwritingLanguage(pack_result.language_code);
  change.status = LanguagePackResultToExtensionStatus(pack_result);

  base::Value::List args = OnLanguagePackStatusChanged::Create(change);

  auto event = std::make_unique<extensions::Event>(
      extensions::events::INPUT_METHOD_PRIVATE_ON_LANGUAGE_PACK_STATUS_CHANGED,
      OnLanguagePackStatusChanged::kEventName, std::move(args), context_);
  router->BroadcastEvent(std::move(event));
}

}  // namespace chromeos
