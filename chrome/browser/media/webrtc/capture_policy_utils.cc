// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/capture_policy_utils.h"

#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/mojom/multi_capture_service.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/multi_capture_service_ash.h"
#include "chrome/browser/ash/policy/multi_screen_capture/multi_screen_capture_policy_service.h"
#include "chrome/browser/ash/policy/multi_screen_capture/multi_screen_capture_policy_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

#if BUILDFLAG(IS_CHROMEOS)
crosapi::mojom::MultiCaptureService* g_multi_capture_service_for_testing =
    nullptr;

void IsMultiCaptureAllowedForAnyOriginOnMainProfileResultReceived(
    base::OnceCallback<void(bool)> callback,
    content::BrowserContext* context,
    bool is_multi_capture_allowed_for_any_origin_on_main_profile) {
  // If the new MultiScreenCaptureAllowedForUrls policy permits access, exit
  // early. If not, check the legacy
  // GetDisplayMediaSetSelectAllScreensAllowedForUrls policy.
  if (is_multi_capture_allowed_for_any_origin_on_main_profile) {
    std::move(callback).Run(true);
    return;
  }

  // TODO(b/329064666): Remove the checks below once the pivot to IWAs is
  // complete.
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    std::move(callback).Run(false);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // To ensure that a user is informed at login time that capturing of all
  // screens can happen (for privacy reasons), this API is only available on
  // primary profiles.
  if (!profile->IsMainProfile()) {
    std::move(callback).Run(false);
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (!host_content_settings_map) {
    std::move(callback).Run(false);
    return;
  }
  ContentSettingsForOneType content_settings =
      host_content_settings_map->GetSettingsForOneType(
          ContentSettingsType::ALL_SCREEN_CAPTURE);
  std::move(callback).Run(base::ranges::any_of(
      content_settings, [](const ContentSettingPatternSource& source) {
        return source.GetContentSetting() ==
               ContentSetting::CONTENT_SETTING_ALLOW;
      }));
}

void CheckAllScreensMediaAllowedForIwaResultReceived(
    base::OnceCallback<void(bool)> callback,
    const GURL& url,
    content::BrowserContext* context,
    bool result) {
  if (result) {
    std::move(callback).Run(true);
    return;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    std::move(callback).Run(false);
    return;
  }
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (!host_content_settings_map) {
    std::move(callback).Run(false);
    return;
  }
  ContentSetting auto_accept_enabled =
      host_content_settings_map->GetContentSetting(
          url, url, ContentSettingsType::ALL_SCREEN_CAPTURE);
  std::move(callback).Run(auto_accept_enabled ==
                          ContentSetting::CONTENT_SETTING_ALLOW);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

namespace capture_policy {

// This pref connects to the GetDisplayMediaSetSelectAllScreensAllowedForUrls
// policy. To avoid dynamic refresh, this pref will not be read directly, but
// the value will be copied manually to the
// kManagedAccessToGetAllScreensMediaInSessionAllowedForUrls pref, which is then
// consumed by content settings to check if access to `getAllScreensMedia` shall
// be permitted for a given origin.
// TODO(b/329064666): Remove this pref once the pivot to IWAs is complete.
const char kManagedAccessToGetAllScreensMediaAllowedForUrls[] =
    "profile.managed_access_to_get_all_screens_media_allowed_for_urls";

#if BUILDFLAG(IS_CHROMEOS_ASH)

// This pref connects to the MultiScreenCaptureAllowedForUrls policy and will
// replace the deprecated GetDisplayMediaSetSelectAllScreensAllowedForUrls
// policy once the pivot to IWAs is complete.
const char kManagedMultiScreenCaptureAllowedForUrls[] =
    "profile.managed_multi_screen_capture_allowed_for_urls";

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

struct RestrictedCapturePolicy {
  const char* pref_name;
  AllowedScreenCaptureLevel capture_level;
};

}  // namespace

#if BUILDFLAG(IS_CHROMEOS)
void SetMultiCaptureServiceForTesting(
    crosapi::mojom::MultiCaptureService* service) {
  CHECK_IS_TEST();
  CHECK(!service || !g_multi_capture_service_for_testing);
  g_multi_capture_service_for_testing = service;
}

crosapi::mojom::MultiCaptureService* GetMultiCaptureService() {
  if (g_multi_capture_service_for_testing) {
    return g_multi_capture_service_for_testing;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
  const int multi_capture_service_version =
      lacros_service
          ->GetInterfaceVersion<crosapi::mojom::MultiCaptureService>();
  if (multi_capture_service_version >=
      static_cast<int>(crosapi::mojom::MultiCaptureService::MethodMinVersions::
                           kIsMultiCaptureAllowedMinVersion)) {
    return lacros_service->GetRemote<crosapi::mojom::MultiCaptureService>()
        .get();
  }
  return nullptr;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->multi_capture_service_ash();
#endif
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsOriginInList(const GURL& request_origin,
                    const base::Value::List& allowed_origins) {
  // Though we are not technically a Content Setting, ContentSettingsPattern
  // aligns better than URLMatcher with the rules from:
  // https://chromeenterprise.google/policies/url-patterns/.
  for (const auto& value : allowed_origins) {
    if (!value.is_string()) {
      continue;
    }
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(value.GetString());
    if (pattern.IsValid() && pattern.Matches(request_origin)) {
      return true;
    }
  }

  return false;
}

AllowedScreenCaptureLevel GetAllowedCaptureLevel(
    const GURL& request_origin,
    content::WebContents* capturer_web_contents) {
  // Since the UI for capture doesn't clip against picture in picture windows
  // properly on all platforms, and since it's not clear that we actually want
  // to support this anyway, turn it off for now.  Note that direct calls into
  // `GetAllowedCaptureLevel(..., PrefService)` will miss this check.
  if (!base::FeatureList::IsEnabled(media::kDocumentPictureInPictureCapture) &&
      PictureInPictureWindowManager::IsChildWebContents(
          capturer_web_contents)) {
    return AllowedScreenCaptureLevel::kDisallowed;
  }

  // If we can't get the PrefService, then we won't apply any restrictions.
  Profile* profile =
      Profile::FromBrowserContext(capturer_web_contents->GetBrowserContext());
  if (!profile) {
    return AllowedScreenCaptureLevel::kUnrestricted;
  }

  const PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return AllowedScreenCaptureLevel::kUnrestricted;
  }

  return GetAllowedCaptureLevel(request_origin, *prefs);
}

AllowedScreenCaptureLevel GetAllowedCaptureLevel(const GURL& request_origin,
                                                 const PrefService& prefs) {
  // Walk through the different "levels" of restriction in priority order. If
  // an origin is in a more restrictive list, it is more restricted.
  // Note that we only store the pref name and not the pref value here, as we
  // want to look the pref value up each time (thus meaning we could not make
  // this a static), as the value can change.
  static constexpr std::array<RestrictedCapturePolicy, 4>
      kScreenCapturePolicyLists{{{prefs::kSameOriginTabCaptureAllowedByOrigins,
                                  AllowedScreenCaptureLevel::kSameOrigin},
                                 {prefs::kTabCaptureAllowedByOrigins,
                                  AllowedScreenCaptureLevel::kTab},
                                 {prefs::kWindowCaptureAllowedByOrigins,
                                  AllowedScreenCaptureLevel::kWindow},
                                 {prefs::kScreenCaptureAllowedByOrigins,
                                  AllowedScreenCaptureLevel::kDesktop}}};

  for (const auto& policy_list : kScreenCapturePolicyLists) {
    if (IsOriginInList(request_origin, prefs.GetList(policy_list.pref_name))) {
      return policy_list.capture_level;
    }
  }

  // If we've reached this point our origin wasn't in any of the override lists.
  // That means that either everything is allowed or nothing is allowed, based
  // on what |kScreenCaptureAllowed| is set to.
  if (prefs.GetBoolean(prefs::kScreenCaptureAllowed)) {
    return AllowedScreenCaptureLevel::kUnrestricted;
  }

  return AllowedScreenCaptureLevel::kDisallowed;
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kManagedAccessToGetAllScreensMediaAllowedForUrls);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterListPref(kManagedMultiScreenCaptureAllowedForUrls);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void CheckGetAllScreensMediaAllowedForAnyOrigin(
    content::BrowserContext* context,
    base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(IS_CHROMEOS)
  if (crosapi::mojom::MultiCaptureService* multi_capture_service =
          GetMultiCaptureService()) {
    multi_capture_service->IsMultiCaptureAllowedForAnyOriginOnMainProfile(
        base::BindOnce(
            IsMultiCaptureAllowedForAnyOriginOnMainProfileResultReceived,
            std::move(callback), context));
  } else {
    // If the multi capture service is not available with the required version,
    // fall back to the original flow using the deprecated policy.
    IsMultiCaptureAllowedForAnyOriginOnMainProfileResultReceived(
        std::move(callback), context, /*result=*/false);
  }
#else
  std::move(callback).Run(false);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void CheckGetAllScreensMediaAllowed(content::BrowserContext* context,
                                    const GURL& url,
                                    base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    std::move(callback).Run(false);
    return;
  }
  // To ensure that a user is informed at login time that capturing of all
  // screens can happen (for privacy reasons), this API is only available on
  // primary profiles.
  if (!profile->IsMainProfile()) {
    std::move(callback).Run(false);
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS)
  crosapi::mojom::MultiCaptureService* multi_capture_service =
      GetMultiCaptureService();
  if (multi_capture_service) {
    multi_capture_service->IsMultiCaptureAllowed(
        url, base::BindOnce(&CheckAllScreensMediaAllowedForIwaResultReceived,
                            std::move(callback), std::move(url), context));
  } else {
    // If the multi capture service is not available with the required version,
    // fall back to the original flow using the deprecated policy.
    CheckAllScreensMediaAllowedForIwaResultReceived(
        std::move(callback), std::move(url), context, /*result=*/false);
  }
#else
  std::move(callback).Run(false);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if !BUILDFLAG(IS_ANDROID)
bool IsTransientActivationRequiredForGetDisplayMedia(
    content::WebContents* contents) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kGetDisplayMediaRequiresUserActivation)) {
    return false;
  }

  if (!contents) {
    return true;
  }

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (!profile) {
    return true;
  }

  PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return true;
  }

  return !policy::IsOriginInAllowlist(
      contents->GetURL(), prefs,
      prefs::kScreenCaptureWithoutGestureAllowedForOrigins);
}
#endif  // !BUILDFLAG(IS_ANDROID)

DesktopMediaList::WebContentsFilter GetIncludableWebContentsFilter(
    const GURL& request_origin,
    AllowedScreenCaptureLevel capture_level) {
  switch (capture_level) {
    case AllowedScreenCaptureLevel::kDisallowed:
      return base::BindRepeating(
          [](content::WebContents* wc) { return false; });
    case AllowedScreenCaptureLevel::kSameOrigin:
      return base::BindRepeating(
          [](const GURL& request_origin, content::WebContents* web_contents) {
            DCHECK(web_contents);
            return !PictureInPictureWindowManager::IsChildWebContents(
                       web_contents) &&
                   url::IsSameOriginWith(request_origin,
                                         web_contents->GetLastCommittedURL()
                                             .DeprecatedGetOriginAsURL());
          },
          request_origin);
    default:
      return base::BindRepeating([](content::WebContents* web_contents) {
        DCHECK(web_contents);
        return !PictureInPictureWindowManager::IsChildWebContents(web_contents);
      });
  }
}

void FilterMediaList(std::vector<DesktopMediaList::Type>& media_types,
                     AllowedScreenCaptureLevel capture_level) {
  std::erase_if(
      media_types, [capture_level](const DesktopMediaList::Type& type) {
        switch (type) {
          case DesktopMediaList::Type::kNone:
            NOTREACHED();
          // SameOrigin is more restrictive than just Tabs, so as long as
          // at least SameOrigin is allowed, these entries should stay.
          // They should be filtered later by the caller.
          case DesktopMediaList::Type::kCurrentTab:
          case DesktopMediaList::Type::kWebContents:
            return capture_level < AllowedScreenCaptureLevel::kSameOrigin;
          case DesktopMediaList::Type::kWindow:
            return capture_level < AllowedScreenCaptureLevel::kWindow;
          case DesktopMediaList::Type::kScreen:
            return capture_level < AllowedScreenCaptureLevel::kDesktop;
        }
      });
}

#if !BUILDFLAG(IS_ANDROID)
class CaptureTerminatedDialogDelegate : public TabModalConfirmDialogDelegate {
 public:
  explicit CaptureTerminatedDialogDelegate(content::WebContents* web_contents)
      : TabModalConfirmDialogDelegate(web_contents) {}
  ~CaptureTerminatedDialogDelegate() override = default;
  std::u16string GetTitle() override {
    return l10n_util::GetStringUTF16(
        IDS_TAB_CAPTURE_TERMINATED_BY_POLICY_TITLE);
  }

  std::u16string GetDialogMessage() override {
    return l10n_util::GetStringUTF16(IDS_TAB_CAPTURE_TERMINATED_BY_POLICY_TEXT);
  }

  int GetDialogButtons() const override {
    return static_cast<int>(ui::mojom::DialogButton::kOk);
  }
};
#endif

void ShowCaptureTerminatedDialog(content::WebContents* contents) {
#if !BUILDFLAG(IS_ANDROID)
  TabModalConfirmDialog::Create(
      std::make_unique<CaptureTerminatedDialogDelegate>(contents), contents);
#endif
}

}  // namespace capture_policy
