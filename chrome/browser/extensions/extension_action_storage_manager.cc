// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_storage_manager.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/constants.h"
#include "ui/base/layout.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

namespace {

const char kBrowserActionStorageKey[] = "browser_action";
const char kPopupUrlStorageKey[] = "poupup_url";
const char kTitleStorageKey[] = "title";
const char kIconStorageKey[] = "icon";
const char kBadgeTextStorageKey[] = "badge_text";
const char kBadgeBackgroundColorStorageKey[] = "badge_background_color";
const char kBadgeTextColorStorageKey[] = "badge_text_color";
const char kAppearanceStorageKey[] = "appearance";

// Only add values to the end of this enum, since it's stored in the user's
// Extension State, under the kAppearanceStorageKey.  It represents the
// ExtensionAction's default visibility.
enum StoredAppearance {
  // The action icon is hidden.
  INVISIBLE = 0,
  // The action is trying to get the user's attention but isn't yet
  // running on the page.  Was only used for script badges.
  OBSOLETE_WANTS_ATTENTION = 1,
  // The action icon is visible with its normal appearance.
  ACTIVE = 2,
};

// Conversion function for reading/writing to storage.
SkColor RawStringToSkColor(const std::string& str) {
  uint64_t value = 0;
  base::StringToUint64(str, &value);
  SkColor color = static_cast<SkColor>(value);
  DCHECK(value == color);  // ensure value fits into color's 32 bits
  return color;
}

// Conversion function for reading/writing to storage.
std::string SkColorToRawString(SkColor color) {
  return base::NumberToString(color);
}

// Conversion function for reading/writing to storage.
bool StringToSkBitmap(const std::string& str, SkBitmap* bitmap) {
  // TODO(mpcomplete): Remove the base64 encode/decode step when
  // http://crbug.com/140546 is fixed.
  std::string raw_str;
  if (!base::Base64Decode(str, &raw_str))
    return false;

  bool success = gfx::PNGCodec::Decode(
      reinterpret_cast<unsigned const char*>(raw_str.data()), raw_str.size(),
      bitmap);
  return success;
}

// Conversion function for reading/writing to storage.
std::string BitmapToString(const SkBitmap& bitmap) {
  std::vector<unsigned char> data;
  bool success = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data);
  if (!success)
    return std::string();

  base::StringPiece raw_str(
      reinterpret_cast<const char*>(&data[0]), data.size());
  std::string base64_str;
  base::Base64Encode(raw_str, &base64_str);
  return base64_str;
}

// Set |action|'s default values to those specified in |dict|.
void SetDefaultsFromValue(const base::DictionaryValue* dict,
                          ExtensionAction* action) {
  const int kDefaultTabId = ExtensionAction::kDefaultTabId;
  std::string str_value;

  // For each value, don't set it if it has been modified already.
  if (dict->GetString(kPopupUrlStorageKey, &str_value) &&
      !action->HasPopupUrl(kDefaultTabId)) {
    action->SetPopupUrl(kDefaultTabId, GURL(str_value));
  }
  if (dict->GetString(kTitleStorageKey, &str_value) &&
      !action->HasTitle(kDefaultTabId)) {
    action->SetTitle(kDefaultTabId, str_value);
  }
  if (dict->GetString(kBadgeTextStorageKey, &str_value) &&
      !action->HasBadgeText(kDefaultTabId)) {
    action->SetBadgeText(kDefaultTabId, str_value);
  }
  if (dict->GetString(kBadgeBackgroundColorStorageKey, &str_value) &&
      !action->HasBadgeBackgroundColor(kDefaultTabId)) {
    action->SetBadgeBackgroundColor(kDefaultTabId,
                                    RawStringToSkColor(str_value));
  }
  if (dict->GetString(kBadgeTextColorStorageKey, &str_value) &&
      !action->HasBadgeTextColor(kDefaultTabId)) {
    action->SetBadgeTextColor(kDefaultTabId, RawStringToSkColor(str_value));
  }

  int appearance_storage = 0;
  if (dict->GetInteger(kAppearanceStorageKey, &appearance_storage) &&
      !action->HasIsVisible(kDefaultTabId)) {
    switch (appearance_storage) {
      case INVISIBLE:
      case OBSOLETE_WANTS_ATTENTION:
        action->SetIsVisible(kDefaultTabId, false);
        break;
      case ACTIVE:
        action->SetIsVisible(kDefaultTabId, true);
        break;
    }
  }

  const base::DictionaryValue* icon_value = NULL;
  if (dict->GetDictionary(kIconStorageKey, &icon_value) &&
      !action->HasIcon(kDefaultTabId)) {
    gfx::ImageSkia icon;
    SkBitmap bitmap;
    for (base::DictionaryValue::Iterator iter(*icon_value); !iter.IsAtEnd();
         iter.Advance()) {
      int icon_size = 0;
      std::string icon_string;
      if (base::StringToInt(iter.key(), &icon_size) &&
          iter.value().GetAsString(&icon_string) &&
          StringToSkBitmap(icon_string, &bitmap)) {
        CHECK(!bitmap.isNull());
        float scale =
            static_cast<float>(icon_size) / ExtensionAction::ActionIconSize();
        icon.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
      }
    }
    action->SetIcon(kDefaultTabId, gfx::Image(icon));
  }
}

// Store |action|'s default values in a DictionaryValue for use in storing to
// disk.
std::unique_ptr<base::DictionaryValue> DefaultsToValue(
    ExtensionAction* action) {
  const int kDefaultTabId = ExtensionAction::kDefaultTabId;
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString(kPopupUrlStorageKey,
                  action->GetPopupUrl(kDefaultTabId).spec());
  dict->SetString(kTitleStorageKey, action->GetTitle(kDefaultTabId));
  dict->SetString(kBadgeTextStorageKey,
                  action->GetExplicitlySetBadgeText(kDefaultTabId));
  dict->SetString(
      kBadgeBackgroundColorStorageKey,
      SkColorToRawString(action->GetBadgeBackgroundColor(kDefaultTabId)));
  dict->SetString(kBadgeTextColorStorageKey,
                  SkColorToRawString(action->GetBadgeTextColor(kDefaultTabId)));
  dict->SetInteger(kAppearanceStorageKey,
                   action->GetIsVisible(kDefaultTabId) ? ACTIVE : INVISIBLE);

  gfx::ImageSkia icon =
      action->GetExplicitlySetIcon(kDefaultTabId).AsImageSkia();
  if (!icon.isNull()) {
    std::unique_ptr<base::DictionaryValue> icon_value(
        new base::DictionaryValue());
    std::vector<gfx::ImageSkiaRep> image_reps = icon.image_reps();
    for (const gfx::ImageSkiaRep& rep : image_reps) {
      int size = static_cast<int>(rep.scale() * icon.width());
      std::string size_string = base::NumberToString(size);
      icon_value->SetString(size_string, BitmapToString(rep.GetBitmap()));
    }
    dict->Set(kIconStorageKey, std::move(icon_value));
  }
  return dict;
}

}  // namespace

ExtensionActionStorageManager::ExtensionActionStorageManager(
    content::BrowserContext* context)
    : browser_context_(context) {
  extension_action_observer_.Add(ExtensionActionAPI::Get(browser_context_));
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));

  StateStore* store = GetStateStore();
  if (store)
    store->RegisterKey(kBrowserActionStorageKey);
}

ExtensionActionStorageManager::~ExtensionActionStorageManager() {
}

void ExtensionActionStorageManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  ExtensionAction* action = ExtensionActionManager::Get(browser_context_)
                                ->GetExtensionAction(*extension);
  if (!action || action->action_type() != ActionInfo::TYPE_BROWSER)
    return;

  StateStore* store = GetStateStore();
  if (store) {
    store->GetExtensionValue(
        extension->id(),
        kBrowserActionStorageKey,
        base::Bind(&ExtensionActionStorageManager::ReadFromStorage,
                   weak_factory_.GetWeakPtr(),
                   extension->id()));
  }
}

void ExtensionActionStorageManager::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  // This is an update to the default settings of the action iff |web_contents|
  // is null. We only persist the default settings to disk, since per-tab
  // settings can't be persisted across browser sessions.
  bool for_default_tab = !web_contents;
  // TODO(devlin): We should probably persist for TYPE_ACTION as well.
  if (browser_context_ == browser_context &&
      extension_action->action_type() == ActionInfo::TYPE_BROWSER &&
      for_default_tab) {
    WriteToStorage(extension_action);
  }
}

void ExtensionActionStorageManager::OnExtensionActionAPIShuttingDown() {
  extension_action_observer_.RemoveAll();
}

void ExtensionActionStorageManager::WriteToStorage(
    ExtensionAction* extension_action) {
  StateStore* store = GetStateStore();
  if (store) {
    std::unique_ptr<base::DictionaryValue> defaults =
        DefaultsToValue(extension_action);
    store->SetExtensionValue(extension_action->extension_id(),
                             kBrowserActionStorageKey, std::move(defaults));
  }
}

void ExtensionActionStorageManager::ReadFromStorage(
    const std::string& extension_id,
    std::unique_ptr<base::Value> value) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)->
      enabled_extensions().GetByID(extension_id);
  if (!extension)
    return;

  ExtensionAction* action = ExtensionActionManager::Get(browser_context_)
                                ->GetExtensionAction(*extension);
  if (!action || action->action_type() != ActionInfo::TYPE_BROWSER) {
    // This can happen if the extension is updated between startup and when the
    // storage read comes back, and the update removes the browser action.
    // http://crbug.com/349371
    return;
  }

  const base::DictionaryValue* dict = NULL;
  if (!value.get() || !value->GetAsDictionary(&dict))
    return;

  SetDefaultsFromValue(dict, action);
}

StateStore* ExtensionActionStorageManager::GetStateStore() {
  return ExtensionSystem::Get(browser_context_)->state_store();
}

}  // namespace extensions
