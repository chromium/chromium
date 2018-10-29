// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/local_ntp_source.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/ntp_icon_source.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search/thumbnail_source.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_list_source.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search.mojom.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search/url_validity_checker_impl.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/gfx/color_utils.h"

namespace {

const char kNtpCustomBackgroundURL[] = "background_url";
const char kNtpCustomBackgroundAttributionLine1[] = "attribution_line_1";
const char kNtpCustomBackgroundAttributionLine2[] = "attribution_line_2";
const char kNtpCustomBackgroundAttributionActionURL[] =
    "attribution_action_url";

// Time in seconds before the UI add/edit custom link dialog automatically
// closes. Keep in sync with custom_edit_dialog.js.
const int kCustomLinkDialogTimeoutSeconds = 2;

base::DictionaryValue GetBackgroundInfoAsDict(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url) {
  base::DictionaryValue background_info;
  background_info.SetKey(kNtpCustomBackgroundURL,
                         base::Value(background_url.spec()));
  background_info.SetKey(kNtpCustomBackgroundAttributionLine1,
                         base::Value(attribution_line_1));
  background_info.SetKey(kNtpCustomBackgroundAttributionLine2,
                         base::Value(attribution_line_2));
  background_info.SetKey(kNtpCustomBackgroundAttributionActionURL,
                         base::Value(action_url.spec()));

  return background_info;
}

std::unique_ptr<base::DictionaryValue> NtpCustomBackgroundDefaults() {
  std::unique_ptr<base::DictionaryValue> defaults =
      std::make_unique<base::DictionaryValue>();
  defaults->SetString(kNtpCustomBackgroundURL, std::string());
  defaults->SetString(kNtpCustomBackgroundAttributionLine1, std::string());
  defaults->SetString(kNtpCustomBackgroundAttributionLine2, std::string());
  defaults->SetString(kNtpCustomBackgroundAttributionActionURL, std::string());
  return defaults;
}

void CopyFileToProfilePath(const base::FilePath& from_path,
                           const base::FilePath& profile_path) {
  base::CopyFile(from_path,
                 profile_path.AppendASCII(
                     chrome::kChromeSearchLocalNtpBackgroundFilename));
}

// In some cases (Sync, upgrading versions) its necessary to check if the file
// actually exists and is in the correct location.
bool CheckLocalBackgroundImageExists(const base::FilePath& profile_path) {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath profile_image =
      profile_path.AppendASCII(chrome::kChromeSearchLocalNtpBackgroundFilename);
  base::FilePath user_data_image = user_data_dir.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename);

  if (base::PathExists(profile_image))
    return true;

  // The image was originally stored in the user data dir, it needs to be moved
  // to the profile path if it's still there.
  if (base::PathExists(user_data_image)) {
    base::CopyFile(user_data_image, profile_image);
    base::DeleteFile(user_data_image, false);
    return true;
  }

  return false;
}

bool IsLocalFileUrl(GURL url) {
  return base::StartsWith(url.spec(),
                          chrome::kChromeSearchLocalNtpBackgroundUrl,
                          base::CompareCase::SENSITIVE);
}

}  // namespace

// Keeps track of any changes in search engine provider and notifies
// InstantService if a third-party search provider (i.e. a third-party NTP) is
// being used.
class InstantService::SearchProviderObserver
    : public TemplateURLServiceObserver {
 public:
  explicit SearchProviderObserver(TemplateURLService* service,
                                  base::RepeatingCallback<void(bool)> callback)
      : service_(service),
        is_google_(search::DefaultSearchProviderIsGoogle(service_)),
        callback_(std::move(callback)) {
    DCHECK(service_);
    service_->AddObserver(this);
  }

  ~SearchProviderObserver() override {
    if (service_)
      service_->RemoveObserver(this);
  }

  bool is_google() { return is_google_; }

 private:
  void OnTemplateURLServiceChanged() override {
    is_google_ = search::DefaultSearchProviderIsGoogle(service_);
    callback_.Run(is_google_);
  }

  void OnTemplateURLServiceShuttingDown() override {
    service_->RemoveObserver(this);
    service_ = nullptr;
  }

  TemplateURLService* service_;
  bool is_google_;
  base::RepeatingCallback<void(bool)> callback_;
};

InstantService::InstantService(Profile* profile)
    : profile_(profile),
      pref_service_(profile_->GetPrefs()),
      weak_ptr_factory_(this) {
  // The initialization below depends on a typical set of browser threads. Skip
  // it if we are running in a unit test without the full suite.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI))
    return;

  // This depends on the existence of the typical browser threads. Therefore it
  // is only instantiated here (after the check for a UI thread above).
  instant_io_context_ = new InstantIOContext();

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());

  most_visited_sites_ = ChromeMostVisitedSitesFactory::NewForProfile(profile_);
  if (most_visited_sites_) {
    bool custom_links_enabled = true;

    // Determine if we are using a third-party NTP. Custom links should only be
    // enabled for the default NTP.
    if (features::IsCustomLinksEnabled()) {
      TemplateURLService* template_url_service =
          TemplateURLServiceFactory::GetForProfile(profile_);
      if (template_url_service) {
        search_provider_observer_ = std::make_unique<SearchProviderObserver>(
            template_url_service,
            base::BindRepeating(&InstantService::OnSearchProviderChanged,
                                weak_ptr_factory_.GetWeakPtr()));
        custom_links_enabled = search_provider_observer_->is_google();
      }
    }

    // 9 tiles are required for the custom links feature in order to balance the
    // Most Visited rows (this is due to an additional "Add" button). Otherwise,
    // Most Visited should return the regular 8 tiles.
    most_visited_sites_->SetMostVisitedURLsObserver(
        this, features::IsCustomLinksEnabled() ? 9 : 8);
    most_visited_sites_->EnableCustomLinks(custom_links_enabled);
  }

  if (profile_ && profile_->GetResourceContext()) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&InstantIOContext::SetUserDataOnIO,
                       profile->GetResourceContext(), instant_io_context_));
  }

  background_service_ = NtpBackgroundServiceFactory::GetForProfile(profile_);

  // Listen for theme installation.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile_)));

  // Set up the data sources that Instant uses on the NTP.
  content::URLDataSource::Add(profile_,
                              std::make_unique<ThemeSource>(profile_));
  content::URLDataSource::Add(profile_,
                              std::make_unique<LocalNtpSource>(profile_));
  content::URLDataSource::Add(profile_,
                              std::make_unique<NtpIconSource>(profile_));
  content::URLDataSource::Add(
      profile_, std::make_unique<ThumbnailSource>(profile_, false));
  content::URLDataSource::Add(
      profile_, std::make_unique<ThumbnailSource>(profile_, true));
  content::URLDataSource::Add(profile_,
                              std::make_unique<ThumbnailListSource>(profile_));
  content::URLDataSource::Add(profile_,
                              std::make_unique<FaviconSource>(profile_));
  content::URLDataSource::Add(profile_,
                              std::make_unique<MostVisitedIframeSource>());

  // Update theme info when the pref is changed via Sync.
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kNtpCustomBackgroundDict,
      base::BindRepeating(&InstantService::UpdateThemeInfo,
                          base::Unretained(this)));
}

InstantService::~InstantService() = default;

void InstantService::AddInstantProcess(int process_id) {
  process_ids_.insert(process_id);

  if (instant_io_context_.get()) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&InstantIOContext::AddInstantProcessOnIO,
                       instant_io_context_, process_id));
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

void InstantService::DeleteMostVisitedItem(const GURL& url) {
  if (most_visited_sites_) {
    most_visited_sites_->AddOrRemoveBlacklistedUrl(url, true);
  }
}

void InstantService::UndoMostVisitedDeletion(const GURL& url) {
  if (most_visited_sites_) {
    most_visited_sites_->AddOrRemoveBlacklistedUrl(url, false);
  }
}

void InstantService::UndoAllMostVisitedDeletions() {
  if (most_visited_sites_) {
    most_visited_sites_->ClearBlacklistedUrls();
  }
}

bool InstantService::AddCustomLink(const GURL& url, const std::string& title) {
  if (most_visited_sites_)
    return most_visited_sites_->AddCustomLink(url, base::UTF8ToUTF16(title));
  return false;
}

bool InstantService::UpdateCustomLink(const GURL& url,
                                      const GURL& new_url,
                                      const std::string& new_title) {
  if (most_visited_sites_) {
    return most_visited_sites_->UpdateCustomLink(url, new_url,
                                                 base::UTF8ToUTF16(new_title));
  }
  return false;
}

bool InstantService::DeleteCustomLink(const GURL& url) {
  if (most_visited_sites_)
    return most_visited_sites_->DeleteCustomLink(url);
  return false;
}

bool InstantService::UndoCustomLinkAction() {
  // Non-Google search providers are not supported.
  if (most_visited_sites_ && search_provider_observer_->is_google()) {
    most_visited_sites_->UndoCustomLinkAction();
    return true;
  }
  return false;
}

bool InstantService::ResetCustomLinks() {
  // Non-Google search providers are not supported.
  if (most_visited_sites_ && search_provider_observer_->is_google()) {
    most_visited_sites_->UninitializeCustomLinks();
    return true;
  }
  return false;
}

void InstantService::DoesUrlResolve(
    const GURL& url,
    chrome::mojom::EmbeddedSearch::DoesUrlResolveCallback callback) {
  if (!features::IsCustomLinksEnabled())
    return;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ntp_custom_link_checker_request", R"(
        semantics {
          sender: "New Tab Page Custom Links"
          description:
            "When a user adds/edits a custom link to the New Tab Page without "
            "specifying the URL scheme, it defaults to HTTPS. This request "
            "checks if the URL resolves with HTTPS; if not, the URL will "
            "default to HTTP instead."
          trigger:
            "When a user adds/edits a custom link without specifying the URL "
            "scheme."
          data: "An HTTP HEAD request to the user specified URL."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be independently disabled in settings, but it "
            "is only activated by direct user action. Note: This will be "
            "disabled if custom links (chrome://flags#ntp-custom-links) is "
            "disabled."
          policy_exception_justification:
            "Not implemented, considered not useful."
        })");

  UrlValidityChecker* url_checker = GetUrlValidityChecker();
  url_checker->DoesUrlResolve(
      url, traffic_annotation,
      base::BindOnce(&InstantService::OnDoesUrlResolveComplete,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void InstantService::UpdateThemeInfo() {
  // Initialize |theme_info_| if necessary.
  if (!theme_info_) {
    BuildThemeInfo();
  }

  ApplyOrResetCustomBackgroundThemeInfo();

  NotifyAboutThemeInfo();
}

void InstantService::UpdateMostVisitedItemsInfo() {
  NotifyAboutMostVisitedItems();
}

void InstantService::SendNewTabPageURLToRenderer(
    content::RenderProcessHost* rph) {
  if (auto* channel = rph->GetChannel()) {
    chrome::mojom::SearchBouncerAssociatedPtr client;
    channel->GetRemoteAssociatedInterface(&client);
    client->SetNewTabPageURL(search::GetNewTabPageURL(profile_));
  }
}

void InstantService::SetCustomBackgroundURL(const GURL& url) {
  SetCustomBackgroundURLWithAttributions(url, std::string(), std::string(),
                                         GURL());
}

void InstantService::SetCustomBackgroundURLWithAttributions(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url) {
  bool is_backdrop_url =
      background_service_ &&
      background_service_->IsValidBackdropUrl(background_url);

  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  RemoveLocalBackgroundImageCopy();

  if (background_url.is_valid() && is_backdrop_url) {
    base::DictionaryValue background_info = GetBackgroundInfoAsDict(
        background_url, attribution_line_1, attribution_line_2, action_url);
    pref_service_->Set(prefs::kNtpCustomBackgroundDict, background_info);
  } else {
    pref_service_->ClearPref(prefs::kNtpCustomBackgroundDict);
  }
}

void InstantService::SetBackgroundToLocalResource() {
  // Add a timestamp to the url to prevent the browser from using a cached
  // version when "Upload an image" is used multiple times.
  std::string time_string = std::to_string(base::Time::Now().ToTimeT());
  std::string local_string(chrome::kChromeSearchLocalNtpBackgroundUrl);
  GURL timestamped_url(local_string + "?ts=" + time_string);

  base::DictionaryValue background_info = GetBackgroundInfoAsDict(
      timestamped_url, std::string(), std::string(), GURL());
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, true);
  pref_service_->Set(prefs::kNtpCustomBackgroundDict, background_info);
}

void InstantService::SelectLocalBackgroundImage(const base::FilePath& path) {
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&CopyFileToProfilePath, path, profile_->GetPath()),
      base::BindOnce(&InstantService::SetBackgroundToLocalResource,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstantService::Shutdown() {
  process_ids_.clear();

  if (instant_io_context_.get()) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&InstantIOContext::ClearInstantProcessesOnIO,
                       instant_io_context_));
  }

  if (most_visited_sites_) {
    most_visited_sites_.reset();
  }

  instant_io_context_ = NULL;
}

void InstantService::OnDoesUrlResolveComplete(
    const GURL& url,
    chrome::mojom::EmbeddedSearch::DoesUrlResolveCallback callback,
    bool resolves,
    base::TimeDelta duration) {
  bool timeout = false;
  if (!resolves) {
    // Internally update the default "https" scheme to "http" if UI dialog has
    // already timed out.
    if (duration >
        base::TimeDelta::FromSeconds(kCustomLinkDialogTimeoutSeconds)) {
      GURL::Replacements replacements;
      replacements.SetSchemeStr(url::kHttpScheme);
      GURL new_url = url.ReplaceComponents(replacements);
      UpdateCustomLink(url, new_url, /*new_title=*/std::string());
      timeout = true;
    }
  }
  std::move(callback).Run(resolves, timeout);
}

void InstantService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      content::RenderProcessHost* rph =
          content::Source<content::RenderProcessHost>(source).ptr();
      Profile* renderer_profile =
          static_cast<Profile*>(rph->GetBrowserContext());
      if (profile_ == renderer_profile)
        SendNewTabPageURLToRenderer(rph);
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      content::RenderProcessHost* rph =
          content::Source<content::RenderProcessHost>(source).ptr();
      Profile* renderer_profile =
          static_cast<Profile*>(rph->GetBrowserContext());
      if (profile_ == renderer_profile)
        OnRendererProcessTerminated(rph->GetID());
      break;
    }
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      theme_info_ = nullptr;
      UpdateThemeInfo();
      break;
    default:
      NOTREACHED() << "Unexpected notification type in InstantService.";
  }
}

void InstantService::OnRendererProcessTerminated(int process_id) {
  process_ids_.erase(process_id);

  if (instant_io_context_.get()) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&InstantIOContext::RemoveInstantProcessOnIO,
                       instant_io_context_, process_id));
  }
}

void InstantService::OnSearchProviderChanged(bool is_google) {
  DCHECK(features::IsCustomLinksEnabled());
  DCHECK(most_visited_sites_);
  most_visited_sites_->EnableCustomLinks(is_google);
}

void InstantService::OnURLsAvailable(
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
  DCHECK(most_visited_sites_);
  most_visited_items_.clear();
  // Use only personalized tiles for instant service.
  const ntp_tiles::NTPTilesVector& tiles =
      sections.at(ntp_tiles::SectionType::PERSONALIZED);
  for (const ntp_tiles::NTPTile& tile : tiles) {
    InstantMostVisitedItem item;
    item.url = tile.url;
    item.title = tile.title;
    item.thumbnail = tile.thumbnail_url;
    item.favicon = tile.favicon_url;
    item.source = tile.source;
    item.title_source = tile.title_source;
    item.data_generation_time = tile.data_generation_time;
    most_visited_items_.push_back(item);
  }

  NotifyAboutMostVisitedItems();
}

void InstantService::OnIconMadeAvailable(const GURL& site_url) {}

void InstantService::NotifyAboutMostVisitedItems() {
  bool is_custom_links = features::IsCustomLinksEnabled() && most_visited_sites_
                             ? most_visited_sites_->IsCustomLinksInitialized()
                             : false;
  for (InstantServiceObserver& observer : observers_)
    observer.MostVisitedItemsChanged(most_visited_items_, is_custom_links);
}

void InstantService::NotifyAboutThemeInfo() {
  for (InstantServiceObserver& observer : observers_)
    observer.ThemeInfoChanged(*theme_info_);
}

namespace {

const int kSectionBorderAlphaTransparency = 80;

// Converts SkColor to RGBAColor
RGBAColor SkColorToRGBAColor(const SkColor& sKColor) {
  RGBAColor color;
  color.r = SkColorGetR(sKColor);
  color.g = SkColorGetG(sKColor);
  color.b = SkColorGetB(sKColor);
  color.a = SkColorGetA(sKColor);
  return color;
}

}  // namespace

void InstantService::BuildThemeInfo() {
  // Get theme information from theme service.
  theme_info_.reset(new ThemeBackgroundInfo());

  // Get if the current theme is the default theme (or GTK+ on linux).
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  theme_info_->using_default_theme =
      theme_service->UsingDefaultTheme() || theme_service->UsingSystemTheme();

  // Get theme colors.
  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile_);
  SkColor background_color =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);
  SkColor text_color = theme_provider.GetColor(ThemeProperties::COLOR_NTP_TEXT);
  SkColor link_color = theme_provider.GetColor(ThemeProperties::COLOR_NTP_LINK);
  SkColor text_color_light =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_TEXT_LIGHT);
  SkColor header_color =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_HEADER);
  // Generate section border color from the header color.
  SkColor section_border_color =
      SkColorSetARGB(kSectionBorderAlphaTransparency,
                     SkColorGetR(header_color),
                     SkColorGetG(header_color),
                     SkColorGetB(header_color));

  // Set colors.
  theme_info_->background_color = SkColorToRGBAColor(background_color);
  theme_info_->text_color = SkColorToRGBAColor(text_color);
  theme_info_->link_color = SkColorToRGBAColor(link_color);
  theme_info_->text_color_light = SkColorToRGBAColor(text_color_light);
  theme_info_->header_color = SkColorToRGBAColor(header_color);
  theme_info_->section_border_color = SkColorToRGBAColor(section_border_color);

  int logo_alternate =
      theme_provider.GetDisplayProperty(ThemeProperties::NTP_LOGO_ALTERNATE);
  theme_info_->logo_alternate = logo_alternate == 1;

  if (theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    // Set theme id for theme background image url.
    theme_info_->theme_id = theme_service->GetThemeID();

    // Set theme background image horizontal alignment.
    int alignment = theme_provider.GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
    if (alignment & ThemeProperties::ALIGN_LEFT)
      theme_info_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_LEFT;
    else if (alignment & ThemeProperties::ALIGN_RIGHT)
      theme_info_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_RIGHT;
    else
      theme_info_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;

    // Set theme background image vertical alignment.
    if (alignment & ThemeProperties::ALIGN_TOP)
      theme_info_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_TOP;
    else if (alignment & ThemeProperties::ALIGN_BOTTOM)
      theme_info_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_BOTTOM;
    else
      theme_info_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;

    // Set theme background image tiling.
    int tiling = theme_provider.GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_TILING);
    switch (tiling) {
      case ThemeProperties::NO_REPEAT:
        theme_info_->image_tiling = THEME_BKGRND_IMAGE_NO_REPEAT;
        break;
      case ThemeProperties::REPEAT_X:
        theme_info_->image_tiling = THEME_BKGRND_IMAGE_REPEAT_X;
        break;
      case ThemeProperties::REPEAT_Y:
        theme_info_->image_tiling = THEME_BKGRND_IMAGE_REPEAT_Y;
        break;
      case ThemeProperties::REPEAT:
        theme_info_->image_tiling = THEME_BKGRND_IMAGE_REPEAT;
        break;
    }

    theme_info_->has_attribution =
        theme_provider.HasCustomImage(IDR_THEME_NTP_ATTRIBUTION);
  }
}

// TODO(crbug.com/863942): Should switching default search provider retain the
// copy of user uploaded photos?
void InstantService::ApplyOrResetCustomBackgroundThemeInfo() {
  // Reset the pref if the feature is disabled.
  if (!features::IsCustomBackgroundsEnabled()) {
    ResetCustomBackgroundThemeInfo();
    return;
  }

  // Custom backgrounds for non-Google search providers are not supported.
  if (!search::DefaultSearchProviderIsGoogle(profile_)) {
    ResetCustomBackgroundThemeInfo();
    return;
  }

  // Attempt to get custom background URL from preferences.
  const base::DictionaryValue* background_info =
      pref_service_->GetDictionary(prefs::kNtpCustomBackgroundDict);
  const base::Value* background_url =
      background_info->FindKey(kNtpCustomBackgroundURL);
  if (!background_url) {
    ResetCustomBackgroundThemeInfo();
    return;
  }

  // Verify that the custom background URL is valid.
  GURL custom_background_url(
      background_info->FindKey(kNtpCustomBackgroundURL)->GetString());
  if (!custom_background_url.is_valid()) {
    ResetCustomBackgroundThemeInfo();
    return;
  }

  // If the url points to a local image check that it actually exists, if not
  // fall back to the default background.
  const PrefService::Preference* background_local_to_device_pref =
      pref_service_->FindPreference(prefs::kNtpCustomBackgroundLocalToDevice);
  if (IsLocalFileUrl(custom_background_url) &&
      background_local_to_device_pref->IsDefaultValue()) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&CheckLocalBackgroundImageExists, profile_->GetPath()),
        base::BindOnce(
            &InstantService::ApplyCustomBackgroundThemeInfoFromLocalFile,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  ApplyCustomBackgroundThemeInfo();
}

void InstantService::ApplyCustomBackgroundThemeInfoFromLocalFile(
    bool file_exists) {
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice,
                            file_exists);

  ApplyCustomBackgroundThemeInfo();

  // This method executes as a result of PostTask... so the call to
  // NotifyAboutThemeInfo from UpdateThemeInfo is lost, explicitly send it
  // here instead.
  NotifyAboutThemeInfo();
}

void InstantService::ApplyCustomBackgroundThemeInfo() {
  const base::DictionaryValue* background_info =
      pref_service_->GetDictionary(prefs::kNtpCustomBackgroundDict);
  GURL custom_background_url(
      background_info->FindKey(kNtpCustomBackgroundURL)->GetString());

  // If a background was set using a local image on a different device fallback
  // to the default background.
  if (IsLocalFileUrl(custom_background_url) &&
      !pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice)) {
    FallbackToDefaultThemeInfo();
    return;
  }

  // Set custom background information in theme info (attributions are
  // optional).
  const base::Value* attribution_line_1 =
      background_info->FindKey(kNtpCustomBackgroundAttributionLine1);
  const base::Value* attribution_line_2 =
      background_info->FindKey(kNtpCustomBackgroundAttributionLine2);
  const base::Value* attribution_action_url =
      background_info->FindKey(kNtpCustomBackgroundAttributionActionURL);

  theme_info_->custom_background_url = custom_background_url;

  if (attribution_line_1) {
    theme_info_->custom_background_attribution_line_1 =
        background_info->FindKey(kNtpCustomBackgroundAttributionLine1)
            ->GetString();
  }
  if (attribution_line_2) {
    theme_info_->custom_background_attribution_line_2 =
        background_info->FindKey(kNtpCustomBackgroundAttributionLine2)
            ->GetString();
  }
  if (attribution_action_url) {
    GURL action_url(
        background_info->FindKey(kNtpCustomBackgroundAttributionActionURL)
            ->GetString());

    if (!action_url.SchemeIsCryptographic()) {
      theme_info_->custom_background_attribution_action_url = GURL();
    } else {
      theme_info_->custom_background_attribution_action_url = action_url;
    }
  }
}

void InstantService::ResetCustomBackgroundThemeInfo() {
  pref_service_->ClearPref(prefs::kNtpCustomBackgroundDict);
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  RemoveLocalBackgroundImageCopy();
  FallbackToDefaultThemeInfo();
}

void InstantService::FallbackToDefaultThemeInfo() {
  theme_info_->custom_background_url = GURL();
  theme_info_->custom_background_attribution_line_1 = std::string();
  theme_info_->custom_background_attribution_line_2 = std::string();
  theme_info_->custom_background_attribution_action_url = GURL();
}

void InstantService::RemoveLocalBackgroundImageCopy() {
  base::FilePath path = profile_->GetPath().AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename);
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(IgnoreResult(&base::DeleteFile), path, false));
}

UrlValidityChecker* InstantService::GetUrlValidityChecker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (url_checker_for_testing_ != nullptr)
    return url_checker_for_testing_;
  static base::NoDestructor<UrlValidityCheckerImpl> checker(
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      base::DefaultTickClock::GetInstance());
  return checker.get();
}

void InstantService::AddValidBackdropUrlForTesting(const GURL& url) const {
  background_service_->AddValidBackdropUrlForTesting(url);
}

// static
void InstantService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      prefs::kNtpCustomBackgroundDict, NtpCustomBackgroundDefaults(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kNtpCustomBackgroundLocalToDevice,
                                false);
}
