// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/font_prewarmer_tab_helper.h"

#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/font_prewarmer.mojom.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace {

const char kSearchResultsPageFontsPref[] =
    "cached_fonts.search_results_page.fonts";

// Key used to associate FontPrewarmerProfileState with BrowserContext.
const void* const kUserDataKey = &kUserDataKey;

// Returns the font names previously stored to the specified key.
std::vector<std::string> GetFontNamesFromPrefsForKey(Profile* profile,
                                                     const char* pref_name) {
  const base::Value::List& font_name_list =
      profile->GetPrefs()->GetList(pref_name);
  if (font_name_list.empty())
    return {};

  std::vector<std::string> font_names;
  for (const auto& font_name_value : font_name_list) {
    if (const std::string* font_name = font_name_value.GetIfString())
      font_names.push_back(*font_name);
  }
  return font_names;
}

// Saves font names to prefs.
void SaveFontNamesToPref(Profile* profile,
                         const char* pref_name,
                         const std::vector<std::string>& font_family_names) {
  base::Value::List font_family_names_values;
  for (auto& name : font_family_names)
    font_family_names_values.Append(name);
  profile->GetPrefs()->SetList(pref_name, std::move(font_family_names_values));
}

// FontPrewarmerCoordinator is responsible for coordinating with the renderer
// to request the fonts used by a page as well as prewarm the last set of fonts
// used. There is one FontPrewarmerCoordinator per Profile.
class FontPrewarmerCoordinator : public base::SupportsUserData::Data,
                                 public content::RenderProcessHostObserver {
 public:
  using RemoteFontPrewarmer = mojo::Remote<chrome::mojom::FontPrewarmer>;

  explicit FontPrewarmerCoordinator(Profile* profile) : profile_(profile) {}

  FontPrewarmerCoordinator(const FontPrewarmerCoordinator&) = delete;
  FontPrewarmerCoordinator& operator=(const FontPrewarmerCoordinator&) = delete;

  ~FontPrewarmerCoordinator() override {
    for (content::RenderProcessHost* rph : prewarmed_hosts_)
      rph->RemoveObserver(this);
  }

  static FontPrewarmerCoordinator& ForProfile(Profile* profile) {
    FontPrewarmerCoordinator* instance = static_cast<FontPrewarmerCoordinator*>(
        profile->GetUserData(kUserDataKey));
    if (!instance) {
      profile->SetUserData(kUserDataKey,
                           std::make_unique<FontPrewarmerCoordinator>(profile));
      instance = static_cast<FontPrewarmerCoordinator*>(
          profile->GetUserData(kUserDataKey));
    }
    return *instance;
  }

  // Requests the renderer to prewarm the last set of fonts used for displaying
  // a search page. Prewarming is done at most once per RenderProcessHost.
  void SendFontsToPrewarm(content::RenderProcessHost* rph) {
    // Only need to prewarm a particular host once.
    if (prewarmed_hosts_.count(rph))
      return;

    // The following code may early out. Insert the entry to ensure an early out
    // doesn't attempt to send the fonts again.
    prewarmed_hosts_.insert(rph);
    rph->AddObserver(this);

    std::vector<std::string> font_names =
        GetFontNamesFromPrefsForKey(profile_, kSearchResultsPageFontsPref);
    if (font_names.empty()) {
      return;
    }

    RemoteFontPrewarmer remote_font_prewarmer;
    rph->BindReceiver(remote_font_prewarmer.BindNewPipeAndPassReceiver());
    remote_font_prewarmer->PrewarmFonts(std::move(font_names));
  }

  // Requests the set of fonts needed to display a search page from `rfh`.
  void RequestFonts(content::RenderFrameHost* rfh) {
    mojo::AssociatedRemote<chrome::mojom::RenderFrameFontFamilyAccessor>
        font_family_accessor;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&font_family_accessor);
    auto* font_family_accessor_raw = font_family_accessor.get();
    // Pass ownership of the remote to the callback as otherwise the callback
    // will never be run (because the mojo connection was destroyed).
    font_family_accessor_raw->GetFontFamilyNames(base::BindOnce(
        &FontPrewarmerCoordinator::OnGotFontsForFrame,
        weak_factory_.GetWeakPtr(), std::move(font_family_accessor)));
  }

 private:
  void OnGotFontsForFrame(
      mojo::AssociatedRemote<chrome::mojom::RenderFrameFontFamilyAccessor>
          font_family_accessor,
      const std::vector<std::string>& font_names) {
    // TODO(sky): add some metrics here so that we know how often the
    // fonts change.
    SaveFontNamesToPref(profile_, kSearchResultsPageFontsPref, font_names);
  }

  // content::RenderProcessHostObserver:
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override {
    host->RemoveObserver(this);
    prewarmed_hosts_.erase(host);
  }

  raw_ptr<Profile> profile_;
  // Set of hosts that were requested to be prewarmed.
  std::set<raw_ptr<content::RenderProcessHost, SetExperimental>>
      prewarmed_hosts_;
  base::WeakPtrFactory<FontPrewarmerCoordinator> weak_factory_{this};
};

}  // namespace

// static
void FontPrewarmerTabHelper::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kSearchResultsPageFontsPref);
}

FontPrewarmerTabHelper::~FontPrewarmerTabHelper() = default;

FontPrewarmerTabHelper::FontPrewarmerTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<FontPrewarmerTabHelper>(*web_contents) {}

// static
std::string FontPrewarmerTabHelper::GetSearchResultsPageFontsPref() {
  return kSearchResultsPageFontsPref;
}

// static
std::vector<std::string> FontPrewarmerTabHelper::GetFontNames(
    Profile* profile) {
  return GetFontNamesFromPrefsForKey(profile, kSearchResultsPageFontsPref);
}

Profile* FontPrewarmerTabHelper::GetProfile() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

bool FontPrewarmerTabHelper::IsSearchResultsPageNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return false;

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(GetProfile());
  return template_url_service &&
         template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
             navigation_handle->GetURL());
}

void FontPrewarmerTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!IsSearchResultsPageNavigation(navigation_handle))
    return;

  const int expected_render_process_host_id =
      navigation_handle->GetExpectedRenderProcessHostId();
  if (expected_render_process_host_id ==
      content::ChildProcessHost::kInvalidUniqueID) {
    expected_render_process_host_id_.reset();
  } else {
    expected_render_process_host_id_ = expected_render_process_host_id;
    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(expected_render_process_host_id);
    DCHECK(rph);
    FontPrewarmerCoordinator::ForProfile(GetProfile()).SendFontsToPrewarm(rph);
  }
}

void FontPrewarmerTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!IsSearchResultsPageNavigation(navigation_handle))
    return;

  content::RenderFrameHost* rfh = navigation_handle->GetRenderFrameHost();
  DCHECK(rfh);
  FontPrewarmerCoordinator& coordinator =
      FontPrewarmerCoordinator::ForProfile(GetProfile());
  if (expected_render_process_host_id_ != rfh->GetProcess()->GetID())
    coordinator.SendFontsToPrewarm(rfh->GetProcess());
  coordinator.RequestFonts(rfh);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FontPrewarmerTabHelper);
