// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <stddef.h>
#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/search.mojom.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/ntp_tiles/constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search/search_provider_observer.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

InstantService::InstantService(Profile* profile)
    : profile_(profile),
      most_visited_info_(std::make_unique<InstantMostVisitedInfo>()),
      pref_service_(profile_->GetPrefs()),
      native_theme_(ui::NativeTheme::GetInstanceForNativeUi()),
      background_updated_timestamp_(base::TimeTicks::Now()) {
  // The initialization below depends on a typical set of browser threads. Skip
  // it if we are running in a unit test without the full suite.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI))
    return;

  most_visited_sites_ = ChromeMostVisitedSitesFactory::NewForProfile(profile_);
  if (most_visited_sites_) {
    most_visited_sites_->EnableCustomLinks(false);
    most_visited_sites_->AddMostVisitedURLsObserver(
        this, ntp_tiles::kMaxNumMostVisited);
  }

  // Listen for theme installation.
  ThemeServiceFactory::GetForProfile(profile_)->AddObserver(this);

  // TODO(crbug.com/40757220): multiple WebUI pages depend on the theme source
  // without adding it themselves. This is not causing an issue because the
  // theme source is being added here. The source should be added where it is
  // used and then the following can be removed.
  content::URLDataSource::Add(profile_,
                              std::make_unique<ThemeSource>(profile_));

  // Set up the data sources that Instant uses on the NTP.
  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFaviconLegacy));
  content::URLDataSource::Add(profile_,
                              std::make_unique<MostVisitedIframeSource>());

  theme_observation_.Observe(native_theme_.get());
}

InstantService::~InstantService() = default;

void InstantService::AddInstantProcess(content::RenderProcessHost* host) {
  process_ids_.insert(host->GetID());
  // The same process may be added for multiple WebContents. Only observe once.
  if (!host_observation_.IsObservingSource(host)) {
    host_observation_.AddObservation(host);
  }
}

bool InstantService::IsInstantProcess(int process_id) const {
  return process_ids_.find(process_id) != process_ids_.end();
}

void InstantService::AddObserver(InstantServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void InstantService::RemoveObserver(InstantServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void InstantService::OnNewTabPageOpened() {
  if (most_visited_sites_) {
    most_visited_sites_->Refresh();
  }
}

void InstantService::OnThemeChanged() {
  theme_ = nullptr;
  UpdateNtpTheme();
}

void InstantService::DeleteMostVisitedItem(const GURL& url) {
  if (most_visited_sites_) {
    most_visited_sites_->AddOrRemoveBlockedUrl(url, true);
  }
}

void InstantService::UndoMostVisitedDeletion(const GURL& url) {
  if (most_visited_sites_) {
    most_visited_sites_->AddOrRemoveBlockedUrl(url, false);
  }
}

void InstantService::UndoAllMostVisitedDeletions() {
  if (most_visited_sites_) {
    most_visited_sites_->ClearBlockedUrls();
  }
}

void InstantService::UpdateNtpTheme() {
  SetNtpElementsNtpTheme();

  NotifyAboutNtpTheme();
}

void InstantService::UpdateMostVisitedInfo() {
  NotifyAboutMostVisitedInfo();
}

NtpTheme* InstantService::GetInitializedNtpTheme() {
  if (!theme_)
    BuildNtpTheme();
  return theme_.get();
}

void InstantService::SetNativeThemeForTesting(ui::NativeTheme* theme) {
  theme_observation_.Reset();
  native_theme_ = theme;
  theme_observation_.Observe(native_theme_.get());
}

void InstantService::Shutdown() {
  process_ids_.clear();

  if (most_visited_sites_) {
    most_visited_sites_.reset();
  }

  ThemeServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void InstantService::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  Profile* renderer_profile = static_cast<Profile*>(host->GetBrowserContext());
  if (profile_ == renderer_profile) {
    process_ids_.erase(host->GetID());
    host_observation_.RemoveObservation(host);
  }
}

void InstantService::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  DCHECK_EQ(observed_theme, native_theme_);
  // Force the theme information to rebuild so the correct using_dark_colors
  // value is sent to the renderer.
  BuildNtpTheme();
  UpdateNtpTheme();
}

void InstantService::OnURLsAvailable(
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
  DCHECK(most_visited_sites_);
  most_visited_info_->items.clear();
  // Use only personalized tiles for instant service.
  const ntp_tiles::NTPTilesVector& tiles =
      sections.at(ntp_tiles::SectionType::PERSONALIZED);
  for (const ntp_tiles::NTPTile& tile : tiles) {
    InstantMostVisitedItem item;
    item.url = tile.url;
    item.title = tile.title;
    item.favicon = tile.favicon_url;
    most_visited_info_->items.push_back(item);
  }

  NotifyAboutMostVisitedInfo();
}

void InstantService::OnIconMadeAvailable(const GURL& site_url) {}

void InstantService::NotifyAboutMostVisitedInfo() {
  for (InstantServiceObserver& observer : observers_)
    observer.MostVisitedInfoChanged(*most_visited_info_);
}

void InstantService::NotifyAboutNtpTheme() {
  for (InstantServiceObserver& observer : observers_)
    observer.NtpThemeChanged(*theme_);
}

void InstantService::BuildNtpTheme() {
  // Get theme information from theme service.
  theme_ = std::make_unique<NtpTheme>();

  // Get if the current theme is the default theme.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  theme_->using_default_theme = theme_service->UsingDefaultTheme();

  SetNtpElementsNtpTheme();

  if (theme_service->UsingExtensionTheme()) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile_)
            ->enabled_extensions()
            .GetByID(theme_service->GetThemeID());
    if (extension) {
      theme_->theme_id = theme_service->GetThemeID();

      const ui::ThemeProvider& theme_provider =
          ThemeService::GetThemeProviderForProfile(profile_);
      if (theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
        theme_->has_theme_image = true;

        // Set theme background image horizontal alignment.
        int alignment = theme_provider.GetDisplayProperty(
            ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
        if (alignment & ThemeProperties::ALIGN_LEFT)
          theme_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_LEFT;
        else if (alignment & ThemeProperties::ALIGN_RIGHT)
          theme_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_RIGHT;
        else
          theme_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;

        // Set theme background image vertical alignment.
        if (alignment & ThemeProperties::ALIGN_TOP)
          theme_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_TOP;
        else if (alignment & ThemeProperties::ALIGN_BOTTOM)
          theme_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_BOTTOM;
        else
          theme_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;

        // Set theme background image tiling.
        int tiling = theme_provider.GetDisplayProperty(
            ThemeProperties::NTP_BACKGROUND_TILING);
        switch (tiling) {
          case ThemeProperties::NO_REPEAT:
            theme_->image_tiling = THEME_BKGRND_IMAGE_NO_REPEAT;
            break;
          case ThemeProperties::REPEAT_X:
            theme_->image_tiling = THEME_BKGRND_IMAGE_REPEAT_X;
            break;
          case ThemeProperties::REPEAT_Y:
            theme_->image_tiling = THEME_BKGRND_IMAGE_REPEAT_Y;
            break;
          case ThemeProperties::REPEAT:
            theme_->image_tiling = THEME_BKGRND_IMAGE_REPEAT;
            break;
        }

        theme_->has_attribution =
            theme_provider.HasCustomImage(IDR_THEME_NTP_ATTRIBUTION);
      }
    }
  }
}

// static
bool InstantService::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* instant_service = InstantServiceFactory::GetForProfile(
      static_cast<Profile*>(browser_context));

  if (!instant_service)
    return false;

  // The process_id for the navigation request will be -1. If
  // so, allow this request since it's not going to another renderer.
  return render_process_id == -1 ||
         instant_service->IsInstantProcess(render_process_id);
}

void InstantService::SetNtpElementsNtpTheme() {
  NtpTheme* theme = GetInitializedNtpTheme();
  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile_);
  theme->logo_alternate = theme_provider.GetDisplayProperty(
                              ThemeProperties::NTP_LOGO_ALTERNATE) == 1;
}
