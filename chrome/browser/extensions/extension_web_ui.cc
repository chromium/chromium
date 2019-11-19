// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_ui.h"

#include <stddef.h>

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"
#include "extensions/browser/extension_icon_placeholder.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "net/base/file_stream.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"

using content::WebContents;
using extensions::Extension;
using extensions::URLOverrides;

namespace {

// The key to the override value for a page.
const char kEntry[] = "entry";
// The key to whether or not the override is active (i.e., can be used).
// Overrides may be inactive e.g. when an extension is disabled.
const char kActive[] = "active";

// Iterates over |list| and:
// - Converts any entries of the form <entry> to
//   { 'entry': <entry>, 'active': true }.
// - Removes any duplicate entries.
// We do the conversion because we previously stored these values as strings
// rather than objects.
// TODO(devlin): Remove the conversion once everyone's updated.
void InitializeOverridesList(base::ListValue* list) {
  base::ListValue migrated;
  std::set<std::string> seen_entries;
  for (auto& val : *list) {
    std::unique_ptr<base::DictionaryValue> new_dict(
        new base::DictionaryValue());
    std::string entry_name;
    base::DictionaryValue* existing_dict = nullptr;
    if (val.GetAsDictionary(&existing_dict)) {
      bool success = existing_dict->GetString(kEntry, &entry_name);
      if (!success)  // See comment about CHECK(success) in ForEachOverrideList.
        continue;
      new_dict->Swap(existing_dict);
    } else if (val.GetAsString(&entry_name)) {
      new_dict->SetString(kEntry, entry_name);
      new_dict->SetBoolean(kActive, true);
    } else {
      NOTREACHED();
      continue;
    }

    if (seen_entries.count(entry_name) == 0) {
      seen_entries.insert(entry_name);
      migrated.Append(std::move(new_dict));
    }
  }

  list->Swap(&migrated);
}

// Adds |override| to |list|, or, if there's already an entry for the override,
// marks it as active.
void AddOverridesToList(base::ListValue* list, const GURL& override_url) {
  const std::string& spec = override_url.spec();
  for (auto& val : *list) {
    base::DictionaryValue* dict = nullptr;
    std::string entry;
    if (!val.GetAsDictionary(&dict) || !dict->GetString(kEntry, &entry)) {
      NOTREACHED();
      continue;
    }
    if (entry == spec) {
      dict->SetBoolean(kActive, true);
      return;  // All done!
    }
    GURL entry_url(entry);
    if (!entry_url.is_valid()) {
      NOTREACHED();
      continue;
    }
    if (entry_url.host() == override_url.host()) {
      dict->SetBoolean(kActive, true);
      dict->SetString(kEntry, spec);
      return;
    }
  }

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetString(kEntry, spec);
  dict->SetBoolean(kActive, true);
  // Add the entry to the front of the list.
  list->Insert(0, std::move(dict));
}

// Validates that each entry in |list| contains a valid url and points to an
// extension contained in |all_extensions| (and, if not, removes it).
void ValidateOverridesList(const extensions::ExtensionSet* all_extensions,
                           base::ListValue* list) {
  base::ListValue migrated;
  std::set<std::string> seen_hosts;
  for (auto& val : *list) {
    base::DictionaryValue* dict = nullptr;
    std::string entry;
    if (!val.GetAsDictionary(&dict) || !dict->GetString(kEntry, &entry)) {
      NOTREACHED();
      continue;
    }
    std::unique_ptr<base::DictionaryValue> new_dict(
        new base::DictionaryValue());
    new_dict->Swap(dict);
    GURL override_url(entry);
    if (!override_url.is_valid())
      continue;

    if (!all_extensions->GetByID(override_url.host()))
      continue;

    // If we've already seen this extension, remove the entry. Only retain the
    // most recent entry for each extension.
    if (!seen_hosts.insert(override_url.host()).second)
      continue;

    migrated.Append(std::move(new_dict));
  }

  list->Swap(&migrated);
}

// Reloads the page in |web_contents| if it uses the same profile as |profile|
// and if the current URL is a chrome URL.
void UnregisterAndReplaceOverrideForWebContents(const std::string& page,
                                                Profile* profile,
                                                WebContents* web_contents) {
  if (Profile::FromBrowserContext(web_contents->GetBrowserContext()) != profile)
    return;

  GURL url = web_contents->GetURL();
  if (!url.SchemeIs(content::kChromeUIScheme) || url.host_piece() != page)
    return;

  // Don't use Reload() since |url| isn't the same as the internal URL that
  // NavigationController has.
  web_contents->GetController().LoadURL(
      url,
      content::Referrer::SanitizeForRequest(
          url,
          content::Referrer(url, network::mojom::ReferrerPolicy::kDefault)),
      ui::PAGE_TRANSITION_RELOAD, std::string());
}

enum UpdateBehavior {
  UPDATE_DEACTIVATE,  // Mark 'active' as false.
  UPDATE_REMOVE,      // Remove the entry from the list.
};

// Updates the entry (if any) for |override_url| in |overrides_list| according
// to |behavior|. Returns true if anything changed.
bool UpdateOverridesList(base::ListValue* overrides_list,
                         const std::string& override_url,
                         UpdateBehavior behavior) {
  auto iter = std::find_if(
      overrides_list->begin(), overrides_list->end(),
      [&override_url](const base::Value& value) {
        std::string entry;
        const base::DictionaryValue* dict = nullptr;
        return value.GetAsDictionary(&dict) &&
               dict->GetString(kEntry, &entry) && entry == override_url;
      });
  if (iter != overrides_list->end()) {
    switch (behavior) {
      case UPDATE_DEACTIVATE: {
        base::DictionaryValue* dict = nullptr;
        bool success = iter->GetAsDictionary(&dict);
        // See comment about CHECK(success) in ForEachOverrideList.
        if (success) {
          dict->SetBoolean(kActive, false);
          break;
        }
        // Else fall through and erase the broken pref.
        FALLTHROUGH;
      }
      case UPDATE_REMOVE:
        overrides_list->Erase(iter, nullptr);
        break;
    }
    return true;
  }
  return false;
}

// Updates each list referenced in |overrides| according to |behavior|.
void UpdateOverridesLists(Profile* profile,
                          const URLOverrides::URLOverrideMap& overrides,
                          UpdateBehavior behavior) {
  if (overrides.empty())
    return;
  PrefService* prefs = profile->GetPrefs();
  DictionaryPrefUpdate update(prefs, ExtensionWebUI::kExtensionURLOverrides);
  base::DictionaryValue* all_overrides = update.Get();
  for (const auto& page_override_pair : overrides) {
    base::ListValue* page_overrides = nullptr;
    // If it's being unregistered, it should already be in the list.
    if (!all_overrides->GetList(page_override_pair.first, &page_overrides)) {
      NOTREACHED();
      continue;
    }
    if (UpdateOverridesList(page_overrides, page_override_pair.second.spec(),
                            behavior)) {
      // This is the active override, so we need to find all existing
      // tabs for this override and get them to reload the original URL.
      base::Callback<void(WebContents*)> callback =
          base::Bind(&UnregisterAndReplaceOverrideForWebContents,
                     page_override_pair.first, profile);
      extensions::ExtensionTabUtil::ForEachTab(callback);
    }
  }
}

// Run favicon callback with image result. If no favicon was available then
// |image| will be empty.
void RunFaviconCallbackAsync(favicon_base::FaviconResultsCallback callback,
                             const gfx::Image& image) {
  std::vector<favicon_base::FaviconRawBitmapResult> favicon_bitmap_results;

  const std::vector<gfx::ImageSkiaRep>& image_reps =
      image.AsImageSkia().image_reps();
  for (size_t i = 0; i < image_reps.size(); ++i) {
    const gfx::ImageSkiaRep& image_rep = image_reps[i];
    auto bitmap_data = base::MakeRefCounted<base::RefCountedBytes>();
    if (gfx::PNGCodec::EncodeBGRASkBitmap(image_rep.GetBitmap(), false,
                                          &bitmap_data->data())) {
      favicon_base::FaviconRawBitmapResult bitmap_result;
      bitmap_result.bitmap_data = bitmap_data;
      bitmap_result.pixel_size = gfx::Size(image_rep.pixel_width(),
                                            image_rep.pixel_height());
      // Leave |bitmap_result|'s icon URL as the default of GURL().
      bitmap_result.icon_type = favicon_base::IconType::kFavicon;

      favicon_bitmap_results.push_back(bitmap_result);
    } else {
      NOTREACHED() << "Could not encode extension favicon";
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(favicon_bitmap_results)));
}

bool ValidateOverrideURL(const base::Value* override_url_value,
                         const GURL& source_url,
                         const extensions::ExtensionSet& extensions,
                         GURL* override_url,
                         const Extension** extension) {
  const base::DictionaryValue* dict = nullptr;
  std::string override;
  bool is_active = false;
  if (!override_url_value || !override_url_value->GetAsDictionary(&dict) ||
      !dict->GetBoolean(kActive, &is_active) || !is_active ||
      !dict->GetString(kEntry, &override)) {
    return false;
  }
  if (!source_url.query().empty())
    override += "?" + source_url.query();
  if (!source_url.ref().empty())
    override += "#" + source_url.ref();
  *override_url = GURL(override);
  if (!override_url->is_valid()) {
    return false;
  }
  *extension = extensions.GetByID(override_url->host());
  if (!*extension) {
    return false;
  }
  return true;
}

// Fetches each list in the overrides dictionary and runs |callback| on it.
void ForEachOverrideList(
    Profile* profile,
    const base::Callback<void(base::ListValue*)>& callback) {
  PrefService* prefs = profile->GetPrefs();
  DictionaryPrefUpdate update(prefs, ExtensionWebUI::kExtensionURLOverrides);
  base::DictionaryValue* all_overrides = update.Get();

  // DictionaryValue::Iterator cannot be used to modify the list. Generate the
  // set of keys instead.
  std::vector<std::string> keys;
  for (base::DictionaryValue::Iterator iter(*all_overrides);
       !iter.IsAtEnd(); iter.Advance()) {
    keys.push_back(iter.key());
  }
  for (const std::string& key : keys) {
    base::ListValue* list = nullptr;
    bool success = all_overrides->GetList(key, &list);
    // In a perfect world, we could CHECK(success) here. Unfortunately, if a
    // user's prefs are mangled (by malware, user modification, hard drive
    // corruption, evil robots, etc), this will fail. Instead, delete the pref.
    if (!success) {
      all_overrides->Remove(key, nullptr);
      continue;
    }
    callback.Run(list);
  }
}

}  // namespace

const char ExtensionWebUI::kExtensionURLOverrides[] =
    "extensions.chrome_url_overrides";

// static
void ExtensionWebUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kExtensionURLOverrides);
}

// static
bool ExtensionWebUI::HandleChromeURLOverride(
    GURL* url,
    content::BrowserContext* browser_context) {
  if (!url->SchemeIs(content::kChromeUIScheme))
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  const base::DictionaryValue* overrides =
      profile->GetPrefs()->GetDictionary(kExtensionURLOverrides);

  std::string url_host = url->host();
  const base::ListValue* url_list = NULL;
  if (!overrides || !overrides->GetList(url_host, &url_list))
    return false;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context);
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();

  GURL component_url;
  bool found_component_override = false;

  // Iterate over the URL list looking for a suitable override. If a
  // valid non-component override is encountered it is chosen immediately.
  for (size_t i = 0; i < url_list->GetSize(); ++i) {
    const base::Value* val = NULL;
    url_list->Get(i, &val);

    GURL override_url;
    const Extension* extension;
    if (!ValidateOverrideURL(
            val, *url, extensions, &override_url, &extension)) {
      // Invalid overrides are cleaned up on startup.
      continue;
    }

    // We can't handle chrome-extension URLs in incognito mode unless the
    // extension uses split mode.
    bool incognito_override_allowed =
        extensions::IncognitoInfo::IsSplitMode(extension) &&
        extensions::util::IsIncognitoEnabled(extension->id(), profile);
    if (profile->IsOffTheRecord() && !incognito_override_allowed) {
      continue;
    }

    if (!extensions::Manifest::IsComponentLocation(extension->location())) {
      *url = override_url;
      return true;
    }

    if (!found_component_override) {
      found_component_override = true;
      component_url = override_url;
    }
  }

  // If no other non-component overrides were found, use the first known
  // component override, if any.
  if (found_component_override) {
    *url = component_url;
    return true;
  }

  return false;
}

// static
bool ExtensionWebUI::HandleChromeURLOverrideReverse(
    GURL* url, content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const base::DictionaryValue* overrides =
      profile->GetPrefs()->GetDictionary(kExtensionURLOverrides);
  if (!overrides)
    return false;

  // Find the reverse mapping based on the given URL. For example this maps the
  // internal URL
  // chrome-extension://eemcgdkfndhakfknompkggombfjjjeno/main.html#1 to
  // chrome://bookmarks/#1 for display in the omnibox.
  for (base::DictionaryValue::Iterator dict_iter(*overrides);
       !dict_iter.IsAtEnd(); dict_iter.Advance()) {
    const base::ListValue* url_list = nullptr;
    if (!dict_iter.value().GetAsList(&url_list))
      continue;

    for (auto list_iter = url_list->begin(); list_iter != url_list->end();
         ++list_iter) {
      const base::DictionaryValue* dict = nullptr;
      if (!list_iter->GetAsDictionary(&dict))
        continue;
      std::string override;
      if (!dict->GetString(kEntry, &override))
        continue;
      if (base::StartsWith(url->spec(), override,
                           base::CompareCase::SENSITIVE)) {
        GURL original_url(content::kChromeUIScheme + std::string("://") +
                          dict_iter.key() +
                          url->spec().substr(override.length()));
        *url = original_url;
        return true;
      }
    }
  }

  return false;
}

// static
void ExtensionWebUI::InitializeChromeURLOverrides(Profile* profile) {
  ForEachOverrideList(profile, base::Bind(&InitializeOverridesList));
}

// static
void ExtensionWebUI::ValidateChromeURLOverrides(Profile* profile) {
  std::unique_ptr<extensions::ExtensionSet> all_extensions =
      extensions::ExtensionRegistry::Get(profile)
          ->GenerateInstalledExtensionsSet();

  ForEachOverrideList(profile,
                      base::Bind(&ValidateOverridesList, all_extensions.get()));
}

// static
void ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
    Profile* profile,
    const URLOverrides::URLOverrideMap& overrides) {
  if (overrides.empty())
    return;
  PrefService* prefs = profile->GetPrefs();
  DictionaryPrefUpdate update(prefs, kExtensionURLOverrides);
  base::DictionaryValue* all_overrides = update.Get();
  for (const auto& page_override_pair : overrides) {
    base::ListValue* page_overrides_weak = nullptr;
    if (!all_overrides->GetList(page_override_pair.first,
                                &page_overrides_weak)) {
      auto page_overrides = std::make_unique<base::ListValue>();
      page_overrides_weak = page_overrides.get();
      all_overrides->Set(page_override_pair.first, std::move(page_overrides));
    }
    AddOverridesToList(page_overrides_weak, page_override_pair.second);
  }
}

// static
void ExtensionWebUI::DeactivateChromeURLOverrides(
    Profile* profile,
    const URLOverrides::URLOverrideMap& overrides) {
  UpdateOverridesLists(profile, overrides, UPDATE_DEACTIVATE);
}

// static
void ExtensionWebUI::UnregisterChromeURLOverrides(
    Profile* profile,
    const URLOverrides::URLOverrideMap& overrides) {
  UpdateOverridesLists(profile, overrides, UPDATE_REMOVE);
}

// static
void ExtensionWebUI::GetFaviconForURL(
    Profile* profile,
    const GURL& page_url,
    favicon_base::FaviconResultsCallback callback) {
  const Extension* extension = extensions::ExtensionRegistry::Get(
      profile)->enabled_extensions().GetByID(page_url.host());
  if (!extension) {
    RunFaviconCallbackAsync(std::move(callback), gfx::Image());
    return;
  }

  // Fetch resources for all supported scale factors for which there are
  // resources. Load image reps for all supported scale factors (in addition to
  // 1x) immediately instead of in an as needed fashion to be consistent with
  // how favicons are requested for chrome:// and page URLs.
  const std::vector<float>& favicon_scales = favicon_base::GetFaviconScales();
  std::vector<extensions::ImageLoader::ImageRepresentation> info_list;
  for (size_t i = 0; i < favicon_scales.size(); ++i) {
    float scale = favicon_scales[i];
    int pixel_size = static_cast<int>(gfx::kFaviconSize * scale);
    extensions::ExtensionResource icon_resource =
        extensions::IconsInfo::GetIconResource(extension,
                                               pixel_size,
                                               ExtensionIconSet::MATCH_BIGGER);

    ui::ScaleFactor resource_scale_factor = ui::GetSupportedScaleFactor(scale);
    if (!icon_resource.empty()) {
      info_list.push_back(extensions::ImageLoader::ImageRepresentation(
          icon_resource,
          extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
          gfx::Size(pixel_size, pixel_size), resource_scale_factor));
    }
  }

  if (info_list.empty()) {
    // Use the placeholder image when no default icon is available.
    gfx::Image placeholder_image =
        extensions::ExtensionIconPlaceholder::CreateImage(
            extension_misc::EXTENSION_ICON_SMALL, extension->name());
    gfx::ImageSkia placeholder_skia(placeholder_image.AsImageSkia());
    // Ensure the ImageSkia has representation at all scales we would use for
    // favicons.
    std::vector<ui::ScaleFactor> scale_factors = ui::GetSupportedScaleFactors();
    for (const auto& scale_factor : scale_factors) {
      placeholder_skia.GetRepresentation(
          ui::GetScaleForScaleFactor(scale_factor));
    }
    RunFaviconCallbackAsync(std::move(callback), gfx::Image(placeholder_skia));
  } else {
    // LoadImagesAsync actually can run callback synchronously. We want to force
    // async.
    extensions::ImageLoader::Get(profile)->LoadImagesAsync(
        extension, info_list,
        base::BindOnce(&RunFaviconCallbackAsync, std::move(callback)));
  }
}
