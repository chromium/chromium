// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/capture_policy_utils.h"

#include <algorithm>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
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
#include "content/public/browser/render_frame_host.h"
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
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace capture_policy {
namespace {

struct RestrictedCapturePolicy {
  const char* pref_name;
  AllowedScreenCaptureLevel capture_level;
};

}  // namespace

bool IsOriginInList(const GURL& request_origin,
                    const base::ListValue& allowed_origins) {
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
#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterListPref(kManagedMultiScreenCaptureAllowedForUrls);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(ENABLE_SCREEN_CAPTURE)
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
#endif  // BUILDFLAG(ENABLE_SCREEN_CAPTURE)

bool IsTabCaptureAllowed(const url::Origin& request_origin,
                         AllowedScreenCaptureLevel capture_level,
                         content::WebContents* web_contents) {
  if (!web_contents ||
      !IsMediaTypeAllowed(capture_level,
                          content::DesktopMediaID::TYPE_WEB_CONTENTS)) {
    return false;
  }
  if (capture_level != AllowedScreenCaptureLevel::kSameOrigin) {
    return true;
  }
  // Compare against the origin that the tab's document committed, not the
  // origin of its URL. The two differ for e.g. about:blank, which inherits
  // the origin of its creator, and for sandboxed documents and error pages,
  // which have an opaque origin.
  content::RenderFrameHost* const main_frame =
      web_contents->GetPrimaryMainFrame();
  return main_frame &&
         request_origin.IsSameOriginWith(main_frame->GetLastCommittedOrigin());
}

DesktopMediaList::WebContentsFilter GetIncludableWebContentsFilter(
    const GURL& request_origin,
    AllowedScreenCaptureLevel capture_level) {
  return base::BindRepeating(
      [](const url::Origin& request_origin,
         AllowedScreenCaptureLevel capture_level,
         content::WebContents* web_contents) {
        // Document picture-in-picture windows are never offered as a capture
        // source. This is not a restriction of capture policy, which is why
        // it is not part of IsTabCaptureAllowed().
        return IsTabCaptureAllowed(request_origin, capture_level,
                                   web_contents) &&
               !PictureInPictureWindowManager::IsChildWebContents(web_contents);
      },
      url::Origin::Create(request_origin), capture_level);
}

bool IsMediaTypeAllowed(AllowedScreenCaptureLevel capture_level,
                        content::DesktopMediaID::Type media_type) {
  switch (media_type) {
    case content::DesktopMediaID::TYPE_NONE:
      return false;
    case content::DesktopMediaID::TYPE_SCREEN:
      return capture_level >= AllowedScreenCaptureLevel::kDesktop;
    case content::DesktopMediaID::TYPE_WINDOW:
      return capture_level >= AllowedScreenCaptureLevel::kWindow;
    case content::DesktopMediaID::TYPE_WEB_CONTENTS:
      // SameOrigin is more restrictive than just tabs; so as long as at least
      // SameOrigin is allowed, then TYPE_WEB_CONTENTS can be included, and the
      // origins will be filtered for the SameOrigin requirement later.
      return capture_level >= AllowedScreenCaptureLevel::kSameOrigin;
  }
  return false;
}

blink::mojom::MediaStreamRequestResult ValidateSelectedMediaId(
    const content::DesktopMediaID& media_id,
    const url::Origin& request_origin,
    AllowedScreenCaptureLevel capture_level) {
  if (!IsMediaTypeAllowed(capture_level, media_id.type)) {
    return blink::mojom::MediaStreamRequestResult::
        CAPTURE_NOT_ALLOWED_BY_POLICY;
  }

  if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) {
    content::WebContents* const target_web_contents =
        content::WebContents::FromRenderFrameHost(
            content::RenderFrameHost::FromID(
                media_id.web_contents_id.render_process_id,
                media_id.web_contents_id.main_render_frame_id));
    if (!target_web_contents) {
      return blink::mojom::MediaStreamRequestResult::CAPTURED_TAB_DESTROYED;
    }
    // Only capture policy is checked here. Which tabs are offered to the user
    // is up to the picker, and e.g. {preferCurrentTab: true} can select a tab
    // that GetIncludableWebContentsFilter() excludes.
    if (!IsTabCaptureAllowed(request_origin, capture_level,
                             target_web_contents)) {
      return blink::mojom::MediaStreamRequestResult::
          CAPTURE_NOT_ALLOWED_BY_POLICY;
    }
  }

  return blink::mojom::MediaStreamRequestResult::OK;
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
  if (!contents) {
    return;
  }
  TabModalConfirmDialog::Create(
      std::make_unique<CaptureTerminatedDialogDelegate>(contents), contents);
#endif
}

bool CapturerRestrictedToSameOrigin(const url::Origin& request_origin,
                                    content::WebContents* capturer) {
  if (!capturer) {
    return false;
  }
  return GetAllowedCaptureLevel(request_origin.GetURL(), capturer) ==
         AllowedScreenCaptureLevel::kSameOrigin;
}

}  // namespace capture_policy
