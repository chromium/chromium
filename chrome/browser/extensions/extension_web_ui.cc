// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_ui.h"

#include <stddef.h>

#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
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
#include "extensions/common/extension_resource.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "net/base/file_stream.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

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
void InitializeOverridesList(base::Value::List& list) {
  base::Value::List migrated;
  std::set<std::string> seen_entries;
  for (auto& val : list) {
    base::Value::Dict new_dict;
    std::string entry_name;
    if (val.is_dict()) {
      const std::string* tmp = val.GetDict().FindString(kEntry);
      if (!tmp)  // See comment about CHECK(success) in
                 // ForEachOverrideList.
        continue;
      entry_name = *tmp;
      new_dict = val.GetDict().Clone();
    } else if (val.is_string()) {
      entry_name = val.GetString();
      new_dict.Set(kEntry, entry_name);
      new_dict.Set(kActive, true);
    } else {
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    // |entry_name| will be set by this point.
    if (seen_entries.count(entry_name) == 0) {
      seen_entries.insert(entry_name);
      migrated.Append(std::move(new_dict));
    }
  }

  list = std::move(migrated);
}

// Adds |override| to |list|, or, if there's already an entry for the override,
// marks it as active.
void AddOverridesToList(base::Value::List& list, const GURL& override_url) {
  const std::string& spec = override_url.spec();
  for (auto& val : list) {
    std::string* entry = nullptr;
    base::Value::Dict* dict = val.GetIfDict();
    if (dict) {
      entry = dict->FindString(kEntry);
    }
    if (!entry) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    if (*entry == spec) {
      dict->Set(kActive, true);
      return;  // All done!
    }
    GURL entry_url(*entry);
    if (!entry_url.is_valid()) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    if (entry_url.host() == override_url.host()) {
      dict->Set(kActive, true);
      dict->Set(kEntry, spec);
      return;
    }
  }

  base::Value::Dict dict;
  dict.Set(kEntry, spec);
  dict.Set(kActive, true);
  // Add the entry to the front of the list.
  list.Insert(list.begin(), base::Value(std::move(dict)));
}

// Validates that each entry in |list| contains a valid url and points to an
// extension contained in |all_extensions| (and, if not, removes it).
void ValidateOverridesList(const extensions::ExtensionSet* all_extensions,
                           base::Value::List& list) {
  base::Value::List migrated;
  std::set<std::string> seen_hosts;
  for (auto& val : list) {
    std::string* entry = nullptr;
    if (val.is_dict()) {
      entry = val.GetDict().FindString(kEntry);
    }
    if (!entry) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    GURL override_url(*entry);
    if (!override_url.is_valid())
      continue;

    if (!all_extensions->GetByID(override_url.host()))
      continue;

    // If we've already seen this extension, remove the entry. Only retain the
    // most recent entry for each extension.
    if (!seen_hosts.insert(override_url.host()).second)
      continue;

    migrated.Append(val.Clone());
  }

  list = std::move(migrated);
}

// Reloads the page in |web_contents| if it uses the same profile as |profile|
// and if the current URL is a chrome URL.
void UnregisterAndReplaceOverrideForWebContents(const std::string& page,
                                                Profile* profile,
                                                WebContents* web_contents) {
  if (Profile::FromBrowserContext(web_contents->GetBrowserContext()) != profile)
    return;

  const GURL& url = web_contents->GetLastCommittedURL();
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
bool UpdateOverridesList(base::Value::List& overrides_list,
                         const std::string& override_url,
                         UpdateBehavior behavior) {
  auto iter = base::ranges::find_if(
      overrides_list, [&override_url](const base::Value& value) {
        if (!value.is_dict())
          return false;
        const std::string* entry = value.GetDict().FindString(kEntry);
        return entry && *entry == override_url;
      });
  if (iter == overrides_list.end())
    return false;

  switch (behavior) {
    case UPDATE_DEACTIVATE: {
      // See comment about CHECK(success) in ForEachOverrideList.
      if (iter->is_dict()) {
        iter->GetDict().Set(kActive, false);
        break;
      }
      // Else fall through and erase the broken pref.
      [[fallthrough]];
    }
    case UPDATE_REMOVE:
      overrides_list.erase(iter);
      break;
  }
  return true;
}

// Updates each list referenced in |overrides| according to |behavior|.
void UpdateOverridesLists(Profile* profile,
                          const URLOverrides::URLOverrideMap& overrides,
                          UpdateBehavior behavior) {
  if (overrides.empty())
    return;
  PrefService* prefs = profile->GetPrefs();
  ScopedDictPrefUpdate update(prefs, ExtensionWebUI::kExtensionURLOverrides);
  base::Value::Dict& all_overrides = update.Get();
  for (const auto& page_override_pair : overrides) {
    base::Value::List* page_overrides =
        all_overrides.FindList(page_override_pair.first);
    if (!page_overrides) {
      // If it's being unregistered it may or may not be in the list. Eg: On
      // uninstalling an externally loaded extension, which has not been enabled
      // once.
      // But if it's being deactivated, it should already be in the list.
      DCHECK_NE(behavior, UPDATE_DEACTIVATE);
      continue;
    }
    if (UpdateOverridesList(*page_overrides, page_override_pair.second.spec(),
                            behavior)) {
      // This is the active override, so we need to find all existing
      // tabs for this override and get them to reload the original URL.
      base::RepeatingCallback<void(WebContents*)> callback =
          base::BindRepeating(&UnregisterAndReplaceOverrideForWebContents,
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
  for (const gfx::ImageSkiaRep& image_rep : image_reps) {
    auto bitmap_data = base::MakeRefCounted<base::RefCountedBytes>();
    if (gfx::PNGCodec::EncodeBGRASkBitmap(image_rep.GetBitmap(), false,
                                          &bitmap_data->as_vector())) {
      favicon_base::FaviconRawBitmapResult bitmap_result;
      bitmap_result.bitmap_data = bitmap_data;
      bitmap_result.pixel_size = gfx::Size(image_rep.pixel_width(),
                                            image_rep.pixel_height());
      // Leave |bitmap_result|'s icon URL as the default of GURL().
      bitmap_result.icon_type = favicon_base::IconType::kFavicon;

      favicon_bitmap_results.push_back(bitmap_result);
    } else {
      NOTREACHED_IN_MIGRATION() << "Could not encode extension favicon";
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(favicon_bitmap_results)));
}

const Extension* ValidateOverrideURL(const base::Value* override_url_value,
                                     const GURL& source_url,
                                     const extensions::ExtensionSet& extensions,
                                     GURL* override_url) {
  if (!override_url_value || !override_url_value->is_dict() ||
      !override_url_value->GetDict().FindBool(kActive).value_or(false) ||
      !override_url_value->GetDict().FindString(kEntry)) {
    return nullptr;
  }
  const std::string* const_override =
      override_url_value->GetDict().FindString(kEntry);
  std::string override = *const_override;
  if (!source_url.query().empty())
    override += "?" + source_url.query();
  if (!source_url.ref().empty())
    override += "#" + source_url.ref();
  *override_url = GURL(override);
  if (!override_url->is_valid())
    return nullptr;
  return extensions.GetByID(override_url->host());
}

// Fetches each list in the overrides dictionary and runs |callback| on it.
void ForEachOverrideList(
    Profile* profile,
    base::RepeatingCallback<void(base::Value::List&)> callback) {
  PrefService* prefs = profile->GetPrefs();
  ScopedDictPrefUpdate update(prefs, ExtensionWebUI::kExtensionURLOverrides);
  base::Value::Dict& all_overrides = update.Get();

  // We shouldn't modify the list during iteration. Generate the set of keys
  // instead.
  std::vector<std::string> keys;
  for (const auto entry : all_overrides) {
    keys.push_back(entry.first);
  }
  for (const std::string& key : keys) {
    base::Value::List* list = all_overrides.FindList(key);
    // In a perfect world, we could CHECK(list) here. Unfortunately, if a
    // user's prefs are mangled (by malware, user modification, hard drive
    // corruption, evil robots, etc), this will fail. Instead, delete the pref.
    if (!list) {
      all_overrides.Remove(key);
      continue;
    }
    callback.Run(*list);
  }
}

// A helper method to retrieve active overrides for the given |url|, if any. If
// |get_all| is true, this will retrieve all active overrides; otherwise it will
// return the highest-priority one (potentially early-out-ing). The resulting
// vector is ordered by priority.
std::vector<GURL> GetOverridesForChromeURL(
    const GURL& url,
    content::BrowserContext* browser_context,
    bool get_all) {
  // Only chrome: URLs can be overridden like this.
  DCHECK(url.SchemeIs(content::kChromeUIScheme));

  Profile* profile = Profile::FromBrowserContext(browser_context);
  const base::Value::Dict& overrides =
      profile->GetPrefs()->GetDict(ExtensionWebUI::kExtensionURLOverrides);

  const base::Value::List* url_list =
      overrides.FindListByDottedPath(url.host_piece());
  if (!url_list)
    return {};  // No overrides present for this host.

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context);
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();

  // Separate out overrides from non-component extensions (higher priority).
  std::vector<GURL> override_urls;
  std::vector<GURL> component_overrides;

  // Iterate over the URL list looking for suitable overrides.
  for (const auto& value : *url_list) {
    GURL override_url;
    const Extension* extension =
        ValidateOverrideURL(&value, url, extensions, &override_url);
    if (!extension) {
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

    if (extensions::Manifest::IsComponentLocation(extension->location())) {
      component_overrides.push_back(override_url);
    } else {
      override_urls.push_back(override_url);
      if (!get_all) {  // Early out, since the highest-priority was found.
        DCHECK_EQ(1u, override_urls.size());
        return override_urls;
      }
    }
  }

  if (!get_all) {
    // Since component overrides are lower priority, we should only get here if
    // there are no non-component overrides.
    DCHECK(override_urls.empty());
    // Return the highest-priority component override, if any.
    if (component_overrides.size() > 1)
      component_overrides.resize(1);
    return component_overrides;
  }

  override_urls.insert(override_urls.end(),
                       std::make_move_iterator(component_overrides.begin()),
                       std::make_move_iterator(component_overrides.end()));
  return override_urls;
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

  std::vector<GURL> overrides =
      GetOverridesForChromeURL(*url, browser_context, /*get_all=*/false);
  if (overrides.empty())
    return false;

  *url = overrides[0];
  return true;
}

// static
bool ExtensionWebUI::HandleChromeURLOverrideReverse(
    GURL* url, content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const base::Value::Dict& overrides =
      profile->GetPrefs()->GetDict(kExtensionURLOverrides);

  // Find the reverse mapping based on the given URL. For example this maps the
  // internal URL
  // chrome-extension://eemcgdkfndhakfknompkggombfjjjeno/main.html#1 to
  // chrome://bookmarks/#1 for display in the omnibox.
  for (const auto dict_iter : overrides) {
    if (!dict_iter.second.is_list())
      continue;

    for (const auto& list_iter : dict_iter.second.GetList()) {
      const std::string* override = nullptr;
      if (list_iter.is_dict())
        override = list_iter.GetDict().FindString(kEntry);
      if (!override)
        continue;
      if (base::StartsWith(url->spec(), *override,
                           base::CompareCase::SENSITIVE)) {
        GURL original_url(content::kChromeUIScheme + std::string("://") +
                          dict_iter.first +
                          url->spec().substr(override->length()));
        *url = original_url;
        return true;
      }
    }
  }

  return false;
}

// static
const extensions::Extension* ExtensionWebUI::GetExtensionControllingURL(
    const GURL& url,
    content::BrowserContext* browser_context) {
  GURL mutable_url(url);
  if (!HandleChromeURLOverride(&mutable_url, browser_context))
    return nullptr;

  DCHECK_NE(url, mutable_url);
  DCHECK(mutable_url.SchemeIs(extensions::kExtensionScheme));

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context)
          ->enabled_extensions()
          .GetByID(mutable_url.host());
  DCHECK(extension);

  return extension;
}

// static
size_t ExtensionWebUI::GetNumberOfExtensionsOverridingURL(
    const GURL& url,
    content::BrowserContext* browser_context) {
  if (!url.SchemeIs(content::kChromeUIScheme))
    return 0;

  return GetOverridesForChromeURL(url, browser_context, /*get_all=*/true)
      .size();
}

// static
void ExtensionWebUI::InitializeChromeURLOverrides(Profile* profile) {
  ForEachOverrideList(profile, base::BindRepeating(&InitializeOverridesList));
}

// static
void ExtensionWebUI::ValidateChromeURLOverrides(Profile* profile) {
  extensions::ExtensionSet all_extensions =
      extensions::ExtensionRegistry::Get(profile)
          ->GenerateInstalledExtensionsSet();

  ForEachOverrideList(
      profile, base::BindRepeating(&ValidateOverridesList, &all_extensions));
}

// static
void ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
    Profile* profile,
    const URLOverrides::URLOverrideMap& overrides) {
  if (overrides.empty())
    return;
  PrefService* prefs = profile->GetPrefs();
  ScopedDictPrefUpdate update(prefs, kExtensionURLOverrides);
  base::Value::Dict& all_overrides = update.Get();
  for (const auto& page_override_pair : overrides) {
    base::Value::List* page_overrides_weak =
        all_overrides.FindListByDottedPath(page_override_pair.first);
    if (page_overrides_weak == nullptr) {
      page_overrides_weak =
          &all_overrides
               .SetByDottedPath(page_override_pair.first, base::Value::List())
               ->GetList();
    }
    AddOverridesToList(*page_overrides_weak, page_override_pair.second);
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
  for (float scale : favicon_scales) {
    int pixel_size = static_cast<int>(gfx::kFaviconSize * scale);
    extensions::ExtensionResource icon_resource =
        extensions::IconsInfo::GetIconResource(
            extension, pixel_size, ExtensionIconSet::Match::kBigger);

    if (!icon_resource.empty()) {
      ui::ResourceScaleFactor resource_scale_factor =
          ui::GetSupportedResourceScaleFactor(scale);
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
    for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
      placeholder_skia.GetRepresentation(
          ui::GetScaleForResourceScaleFactor(scale_factor));
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
