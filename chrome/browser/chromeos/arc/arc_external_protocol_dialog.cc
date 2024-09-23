// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_external_protocol_dialog.h"

#include <map>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_metrics.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/link_capturing/metrics/intent_handling_metrics.h"
#endif

using content::WebContents;

namespace arc {

namespace {

// The proxy activity for launching an ARC IME's settings activity. These names
// have to be in sync with the ones used in ArcInputMethodManagerService.java on
// the container side. Otherwise, the picker dialog might pop up unexpectedly.
constexpr char kPackageForOpeningArcImeSettingsPage[] =
    "org.chromium.arc.applauncher";
constexpr char kActivityForOpeningArcImeSettingsPage[] =
    "org.chromium.arc.applauncher.InputMethodSettingsActivity";

// Size of device icons in DIPs.
constexpr int kDeviceIconSize = 16;

using IntentPickerResponseWithDevices =
    base::OnceCallback<void(std::vector<SharingTargetDeviceInfo> devices,
                            apps::IntentPickerBubbleType intent_picker_type,
                            const std::string& launch_name,
                            apps::PickerEntryType entry_type,
                            apps::IntentPickerCloseReason close_reason,
                            bool should_persist)>;

// Creates an icon for a specific |device_form_factor|.
ui::ImageModel CreateDeviceIcon(
    const syncer::DeviceInfo::FormFactor device_form_factor) {
  const gfx::VectorIcon& icon =
      device_form_factor == syncer::DeviceInfo::FormFactor::kTablet
          ? kTabletIcon
          : kHardwareSmartphoneIcon;
  return ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, kDeviceIconSize);
}

// Adds |devices| to |picker_entries| and returns the new list. The devices are
// added to the beginning of the list.
std::vector<apps::IntentPickerAppInfo> AddDevices(
    const std::vector<SharingTargetDeviceInfo>& devices,
    std::vector<apps::IntentPickerAppInfo> picker_entries) {
  DCHECK(!devices.empty());

  // First add all devices to the list.
  std::vector<apps::IntentPickerAppInfo> all_entries;
  for (const SharingTargetDeviceInfo& device : devices) {
    all_entries.emplace_back(apps::PickerEntryType::kDevice,
                             CreateDeviceIcon(device.form_factor()),
                             device.guid(), device.client_name());
  }

  // Append the previous list by moving its elements.
  for (auto& entry : picker_entries) {
    all_entries.emplace_back(std::move(entry));
  }

  return all_entries;
}

// Adds remote devices to |app_info| and shows the intent picker dialog if there
// is at least one app or device to choose from.
bool MaybeAddDevicesAndShowPicker(
    const GURL& url,
    const std::optional<url::Origin>& initiating_origin,
    WebContents* web_contents,
    std::vector<apps::IntentPickerAppInfo> app_info,
    bool stay_in_chrome,
    bool show_remember_selection,
    IntentPickerResponseWithDevices callback) {
  Browser* browser =
      web_contents ? chrome::FindBrowserWithTab(web_contents) : nullptr;
  if (!browser) {
    return false;
  }

  bool has_apps = !app_info.empty();
  bool has_devices = false;

  auto bubble_type = apps::IntentPickerBubbleType::kExternalProtocol;
  ClickToCallUiController* controller = nullptr;
  std::vector<SharingTargetDeviceInfo> devices;

  if (ShouldOfferClickToCallForURL(web_contents->GetBrowserContext(), url)) {
    bubble_type = apps::IntentPickerBubbleType::kClickToCall;
    controller =
        ClickToCallUiController::GetOrCreateFromWebContents(web_contents);
    devices = controller->GetDevices();
    has_devices = !devices.empty();
    if (has_devices) {
      app_info = AddDevices(devices, std::move(app_info));
    }
  }

  if (app_info.empty()) {
    return false;
  }

  IntentPickerTabHelper::ShowOrHideIcon(
      web_contents,
      bubble_type == apps::IntentPickerBubbleType::kExternalProtocol);
  browser->window()->ShowIntentPickerBubble(
      std::move(app_info), stay_in_chrome, show_remember_selection, bubble_type,
      initiating_origin,
      base::BindOnce(std::move(callback), std::move(devices), bubble_type));

  if (controller) {
    controller->OnIntentPickerShown(has_devices, has_apps);
  }

  return true;
}

void CloseTabIfNeeded(base::WeakPtr<WebContents> web_contents,
                      bool safe_to_bypass_ui) {
  if (!web_contents) {
    return;
  }

  if (web_contents->GetController().IsInitialNavigation() ||
      safe_to_bypass_ui) {
    web_contents->ClosePage();
  }
}

// Tells whether or not Chrome is an app candidate for the current navigation.
bool IsChromeAnAppCandidate(
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>&
        handlers) {
  for (const auto& handler : handlers) {
    if (handler.package_name == kArcIntentHelperPackageName) {
      return true;
    }
  }
  return false;
}

// Returns true if |handlers| only contains Chrome as an app candidate for the
// current navigation.
bool IsChromeOnlyAppCandidate(
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>&
        handlers) {
  return handlers.size() == 1 && IsChromeAnAppCandidate(handlers);
}

// Returns true if the |handler| is for opening ARC IME settings page.
bool ForOpeningArcImeSettingsPage(
    const ArcIntentHelperMojoDelegate::IntentHandlerInfo& handler) {
  return (handler.package_name == kPackageForOpeningArcImeSettingsPage) &&
         (handler.activity_name == kActivityForOpeningArcImeSettingsPage);
}

// Shows |url| in the current tab.
void OpenUrlInChrome(base::WeakPtr<WebContents> web_contents, const GURL& url) {
  if (!web_contents) {
    return;
  }

  const ui::PageTransition page_transition_type =
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
  constexpr bool kIsRendererInitiated = false;
  const content::OpenURLParams params(
      url,
      content::Referrer(web_contents->GetLastCommittedURL(),
                        network::mojom::ReferrerPolicy::kDefault),
      WindowOpenDisposition::CURRENT_TAB, page_transition_type,
      kIsRendererInitiated);
  web_contents->OpenURL(params, /*navigation_handle_callback=*/{});
}

ArcIntentHelperMojoDelegate::IntentInfo CreateIntentInfo(const GURL& url,
                                                         bool ui_bypassed) {
  // Create an intent with action VIEW, the |url| we are redirecting the user to
  // and a flag that tells whether or not the user interacted with the picker UI

  constexpr char kArcIntentActionView[] = "org.chromium.arc.intent.action.VIEW";
  return ArcIntentHelperMojoDelegate::IntentInfo(
      kArcIntentActionView, /*categories=*/std::nullopt, url.spec(),
      /*type=*/std::nullopt, ui_bypassed, /*extras=*/std::nullopt);
}

// Sends |url| to ARC.
void HandleUrlInArc(base::WeakPtr<WebContents> web_contents,
                    const GurlAndActivityInfo& url_and_activity,
                    bool ui_bypassed,
                    ArcIntentHelperMojoDelegate* mojo_delegate) {
  // ArcIntentHelperMojoDelegate is already varified non-null.
  DCHECK(mojo_delegate);

  // We want to inform ARC about whether or not the user interacted with the
  // picker UI, also since we want to be more explicit about the package and
  // activity we are using, we are relying in HandleIntent() to comunicate back
  // to ARC.
  if (mojo_delegate->HandleIntent(
          CreateIntentInfo(url_and_activity.first, ui_bypassed),
          ArcIntentHelperMojoDelegate::ActivityName(
              std::move(url_and_activity.second.package_name),
              std::move(url_and_activity.second.activity_name)))) {
    CloseTabIfNeeded(web_contents, ui_bypassed);
  }
}

// A helper function called by GetAction().
GetActionResult GetActionInternal(
    const GURL& original_url,
    const ArcIntentHelperMojoDelegate::IntentHandlerInfo& handler,
    GurlAndActivityInfo* out_url_and_activity_name) {
  if (handler.fallback_url.has_value()) {
    *out_url_and_activity_name =
        GurlAndActivityInfo(GURL(*handler.fallback_url),
                            ArcIntentHelperMojoDelegate::ActivityName(
                                handler.package_name, handler.activity_name));
    if (handler.package_name == kArcIntentHelperPackageName) {
      // Since |package_name| is "Chrome", and |fallback_url| is not null, the
      // URL must be either http or https. Check it just in case, and if not,
      // fallback to HANDLE_URL_IN_ARC;
      if (out_url_and_activity_name->first.SchemeIsHTTPOrHTTPS()) {
        return GetActionResult::OPEN_URL_IN_CHROME;
      }

      LOG(WARNING) << "Failed to handle " << out_url_and_activity_name->first
                   << " in Chrome. Falling back to ARC...";
    }
    // |fallback_url| which Chrome doesn't support is passed (e.g. market:).
    return GetActionResult::HANDLE_URL_IN_ARC;
  }

  // Unlike |handler->fallback_url|, the |original_url| should always be handled
  // in ARC since it's external to Chrome.
  *out_url_and_activity_name = GurlAndActivityInfo(
      original_url, ArcIntentHelperMojoDelegate::ActivityName(
                        handler.package_name, handler.activity_name));
  return GetActionResult::HANDLE_URL_IN_ARC;
}

// Gets an action that should be done when ARC has the |handlers| for the
// |original_url| and the user selects |selected_app_index|. When the user
// hasn't selected any app, |selected_app_index| must be set to
// |handlers.size()|.
//
// When the returned action is either OPEN_URL_IN_CHROME or HANDLE_URL_IN_ARC,
// |out_url_and_activity_name| is filled accordingly.
//
// |in_out_safe_to_bypass_ui| is used to reflect whether or not we should
// display the UI: it initially informs whether or not this navigation was
// initiated within ARC, and then gets double-checked and used to store whether
// or not the user can safely bypass the UI.
GetActionResult GetAction(
    const GURL& original_url,
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>& handlers,
    size_t selected_app_index,
    GurlAndActivityInfo* out_url_and_activity_name,
    bool* in_out_safe_to_bypass_ui) {
  DCHECK(out_url_and_activity_name);
  DCHECK(!handlers.empty());

  if (selected_app_index == handlers.size()) {
    // The user hasn't made the selection yet.

    // If |handlers| has only one element and either of the following conditions
    // is met, open the URL in Chrome or ARC without showing the picker UI.
    // 1) its package is "Chrome", open the fallback URL in the current tab
    // without showing the dialog.
    // 2) its package is not "Chrome" but it has been marked as
    // |in_out_safe_to_bypass_ui|, this means that we trust the current tab
    // since its content was originated from ARC.
    // 3) its package and activity are for opening ARC IME settings page. The
    // activity is launched with an explicit user action in chrome://settings.
    if (handlers.size() == 1) {
      const GetActionResult internal_result = GetActionInternal(
          original_url, handlers[0], out_url_and_activity_name);

      if ((internal_result == GetActionResult::HANDLE_URL_IN_ARC &&
           (*in_out_safe_to_bypass_ui ||
            ForOpeningArcImeSettingsPage(handlers[0]))) ||
          internal_result == GetActionResult::OPEN_URL_IN_CHROME) {
        // Make sure the |in_out_safe_to_bypass_ui| flag is actually marked, its
        // maybe not important for OPEN_URL_IN_CHROME but just for consistency.
        *in_out_safe_to_bypass_ui = true;
        return internal_result;
      }
    }

    // Since we have 2+ app candidates we should display the UI, unless there is
    // an already preferred app. |is_preferred| will never be true unless the
    // user explicitly marked it as such.
    *in_out_safe_to_bypass_ui = false;
    for (size_t i = 0; i < handlers.size(); ++i) {
      const ArcIntentHelperMojoDelegate::IntentHandlerInfo& handler =
          handlers[i];
      if (!handler.is_preferred) {
        continue;
      }
      // This is another way to bypass the UI, since the user already expressed
      // some sort of preference.
      *in_out_safe_to_bypass_ui = true;
      // A preferred activity is found. Decide how to open it, either in Chrome
      // or ARC.
      return GetActionInternal(original_url, handler,
                               out_url_and_activity_name);
    }
    // Ask the user to pick one.
    return GetActionResult::ASK_USER;
  }

  // The user already made a selection so this should be false.
  *in_out_safe_to_bypass_ui = false;
  return GetActionInternal(original_url, handlers[selected_app_index],
                           out_url_and_activity_name);
}

// Returns true if the |url| is safe to be forwarded to ARC without showing the
// disambig dialog, besides having this flag set we need to check that there is
// only one app candidate, this is enforced via GetAction(). Any navigation
// coming from ARC via ChromeShellDelegate MUST be marked as such.
//
// Mark as not "safe" (aka return false) on the contrary, most likely those
// cases will require the user to pass thru the intent picker UI.
bool GetAndResetSafeToRedirectToArcWithoutUserConfirmationFlag(
    WebContents* web_contents) {
  const char* key =
      arc::ArcWebContentsData::ArcWebContentsData::kArcTransitionFlag;
  arc::ArcWebContentsData* arc_data =
      static_cast<arc::ArcWebContentsData*>(web_contents->GetUserData(key));
  if (!arc_data) {
    return false;
  }

  web_contents->RemoveUserData(key);
  return true;
}

void HandleDeviceSelection(WebContents* web_contents,
                           const std::vector<SharingTargetDeviceInfo>& devices,
                           const std::string& device_guid,
                           const GURL& url) {
  if (!web_contents) {
    return;
  }

  const auto it =
      base::ranges::find(devices, device_guid, &SharingTargetDeviceInfo::guid);
  CHECK(it != devices.end(), base::NotFatalUntil::M130);
  const SharingTargetDeviceInfo& device = *it;

  ClickToCallUiController::GetOrCreateFromWebContents(web_contents)
      ->OnDeviceSelected(url.GetContent(), device,
                         SharingClickToCallEntryPoint::kLeftClickLink);
}

// Handles |url| if possible. Returns true if it is actually handled.
bool HandleUrl(
    base::WeakPtr<WebContents> web_contents,
    const GURL& url,
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>& handlers,
    size_t selected_app_index,
    GetActionResult* out_result,
    bool safe_to_bypass_ui,
    ArcIntentHelperMojoDelegate* mojo_delegate) {
  GurlAndActivityInfo url_and_activity_name(
      GURL(),
      ArcIntentHelperMojoDelegate::ActivityName{/*package=*/std::string(),
                                                /*activity=*/std::string()});

  const GetActionResult result =
      GetAction(url, handlers, selected_app_index, &url_and_activity_name,
                &safe_to_bypass_ui);
  if (out_result) {
    *out_result = result;
  }

  switch (result) {
    case GetActionResult::OPEN_URL_IN_CHROME:
      OpenUrlInChrome(web_contents, url_and_activity_name.first);
      return true;
    case GetActionResult::HANDLE_URL_IN_ARC:
      HandleUrlInArc(web_contents, url_and_activity_name, safe_to_bypass_ui,
                     mojo_delegate);
      return true;
    case GetActionResult::ASK_USER:
      break;
  }

  return false;
}

// Returns a fallback http(s) in |handlers| which Chrome can handle. Returns
// an empty GURL if none found.
GURL GetUrlToNavigateOnDeactivate(
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>&
        handlers) {
  const GURL empty_url;
  for (size_t i = 0; i < handlers.size(); ++i) {
    GurlAndActivityInfo url_and_package(
        GURL(),
        ArcIntentHelperMojoDelegate::ActivityName{/*package=*/std::string(),
                                                  /*activity=*/std::string()});
    if (GetActionInternal(empty_url, handlers[i], &url_and_package) ==
        GetActionResult::OPEN_URL_IN_CHROME) {
      DCHECK(url_and_package.first.SchemeIsHTTPOrHTTPS());
      return url_and_package.first;
    }
  }
  return empty_url;  // nothing found.
}

// Called when the dialog is just deactivated without pressing one of the
// buttons.
void OnIntentPickerDialogDeactivated(
    base::WeakPtr<WebContents> web_contents,
    bool safe_to_bypass_ui,
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>&
        handlers) {
  const GURL url_to_open_in_chrome = GetUrlToNavigateOnDeactivate(handlers);
  if (url_to_open_in_chrome.is_empty()) {
    CloseTabIfNeeded(web_contents, safe_to_bypass_ui);
  } else {
    OpenUrlInChrome(web_contents, url_to_open_in_chrome);
  }
}

size_t GetAppIndex(
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>&
        app_candidates,
    const std::string& selected_app_package) {
  for (size_t i = 0; i < app_candidates.size(); ++i) {
    if (app_candidates[i].package_name == selected_app_package) {
      return i;
    }
  }
  return app_candidates.size();
}

// Called when the dialog is closed. Note that once we show the UI, we should
// never show the Chrome OS' fallback dialog.
void OnIntentPickerClosed(
    base::WeakPtr<WebContents> web_contents,
    const GURL& url,
    bool safe_to_bypass_ui,
    std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers,
    std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate,
    std::vector<SharingTargetDeviceInfo> devices,
    apps::IntentPickerBubbleType intent_picker_type,
    const std::string& selected_app_package,
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason reason,
    bool should_persist) {
  // ArcIntentHelperMojoDelegate is already varified non-null.
  DCHECK(mojo_delegate);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Even if ArcExternalProtocolDialog shares the same icon on the omnibox as an
  // http(s) request (via AppsNavigationThrottle), the UI here shouldn't stay in
  // the omnibox since the decision should be taken right away in a kind of
  // blocking fashion.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* context = web_contents ? web_contents->GetBrowserContext() : nullptr;
#endif

  if (web_contents) {
    if (intent_picker_type == apps::IntentPickerBubbleType::kClickToCall) {
      ClickToCallUiController::GetOrCreateFromWebContents(web_contents.get())
          ->OnIntentPickerClosed();
    } else {
      IntentPickerTabHelper::ShowOrHideIcon(web_contents.get(),
                                            /*should_show_icon=*/false);
    }
  }

  if (entry_type == apps::PickerEntryType::kDevice) {
    DCHECK_EQ(apps::IntentPickerCloseReason::OPEN_APP, reason);
    DCHECK(!should_persist);
    HandleDeviceSelection(web_contents.get(), devices, selected_app_package,
                          url);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (context) {
      apps::IntentHandlingMetrics::RecordExternalProtocolUserInteractionMetrics(
          context, entry_type, reason, should_persist);
    }
#endif
    return;
  }

  // If the user selected an app to continue the navigation, confirm that the
  // |package_name| matches a valid option and return the index.
  const size_t selected_app_index = GetAppIndex(handlers, selected_app_package);

  // Make sure ARC intent helper instance is connected.
  if (!mojo_delegate->IsArcAvailable()) {
    reason = apps::IntentPickerCloseReason::ERROR_AFTER_PICKER;
  }

  if (reason == apps::IntentPickerCloseReason::OPEN_APP ||
      reason == apps::IntentPickerCloseReason::STAY_IN_CHROME) {
    if (selected_app_index == handlers.size()) {
      // Selected app does not exist.
      reason = apps::IntentPickerCloseReason::ERROR_AFTER_PICKER;
    }
  }

  switch (reason) {
    case apps::IntentPickerCloseReason::OPEN_APP:
      // Only ARC apps are offered in the external protocol intent picker, so if
      // the user decided to open in app the type must be ARC.
      DCHECK_EQ(apps::PickerEntryType::kArc, entry_type);

      if (should_persist) {
        mojo_delegate->AddPreferredPackage(
            handlers[selected_app_index].package_name);
      }

      // Launch the selected app.
      // As the current web page is closed, |web_contents| will be invalidated.
      HandleUrl(web_contents, url, handlers, selected_app_index,
                /*out_result=*/nullptr, safe_to_bypass_ui, mojo_delegate.get());
      break;
    case apps::IntentPickerCloseReason::PREFERRED_APP_FOUND:
      // We shouldn't be here if a preferred app was found.
      NOTREACHED_IN_MIGRATION();
      return;  // no UMA recording.
    case apps::IntentPickerCloseReason::STAY_IN_CHROME:
      LOG(ERROR) << "Chrome is not a valid option for external protocol URLs";
      NOTREACHED_IN_MIGRATION();
      return;  // no UMA recording.
    case apps::IntentPickerCloseReason::ERROR_BEFORE_PICKER:
      // This can happen since an error could occur right before invoking
      // Show() on the bubble's UI code.
      [[fallthrough]];
    case apps::IntentPickerCloseReason::ERROR_AFTER_PICKER:
      LOG(ERROR) << "IntentPickerBubbleView returned CloseReason::ERROR: "
                 << "selected_app_index=" << selected_app_index
                 << ", handlers.size=" << handlers.size();
      [[fallthrough]];
    case apps::IntentPickerCloseReason::DIALOG_DEACTIVATED:
      // The user didn't select any ARC activity.
      OnIntentPickerDialogDeactivated(web_contents, safe_to_bypass_ui,
                                      handlers);
      break;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (context) {
    apps::IntentHandlingMetrics::RecordExternalProtocolUserInteractionMetrics(
        context, entry_type, reason, should_persist);
  }
#endif
}

// Called when ARC returned activity icons for the |handlers|.
void OnAppIconsReceived(
    base::WeakPtr<WebContents> web_contents,
    const GURL& url,
    const std::optional<url::Origin>& initiating_origin,
    bool safe_to_bypass_ui,
    std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers,
    std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate,
    base::OnceCallback<void(bool)> handled_cb,
    bool show_stay_in_chrome,
    std::unique_ptr<ArcIconCacheDelegate::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using AppInfo = apps::IntentPickerAppInfo;
  std::vector<AppInfo> app_info;

  for (const auto& handler : handlers) {
    const ArcIconCacheDelegate::ActivityName activity(handler.package_name,
                                                      handler.activity_name);
    const auto it = icons->find(activity);
    app_info.emplace_back(apps::PickerEntryType::kArc,
                          it != icons->end()
                              ? ui::ImageModel::FromImage(it->second.icon16)
                              : ui::ImageModel(),
                          handler.package_name, handler.name);
  }

  Browser* browser =
      web_contents ? chrome::FindBrowserWithTab(web_contents.get()) : nullptr;

  if (!browser) {
    return std::move(handled_cb).Run(false);
  }

  bool handled = MaybeAddDevicesAndShowPicker(
      url, initiating_origin, web_contents.get(), std::move(app_info),
      show_stay_in_chrome,
      /*show_remember_selection=*/true,
      base::BindOnce(OnIntentPickerClosed, web_contents, url, safe_to_bypass_ui,
                     std::move(handlers), std::move(mojo_delegate)));
  return std::move(handled_cb).Run(handled);
}

void ShowExternalProtocolDialogWithoutApps(
    base::WeakPtr<WebContents> web_contents,
    const GURL& url,
    const std::optional<url::Origin>& initiating_origin,
    std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate,
    base::OnceCallback<void(bool)> handled_cb) {
  // Try to show the device picker and fallback to the default dialog otherwise.
  bool handled = MaybeAddDevicesAndShowPicker(
      url, initiating_origin, web_contents.get(),
      /*app_info=*/{}, /*stay_in_chrome=*/false,
      /*show_remember_selection=*/false,
      base::BindOnce(
          OnIntentPickerClosed, web_contents, url,
          /*safe_to_bypass_ui=*/false,
          std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>(),
          std::move(mojo_delegate)));

  return std::move(handled_cb).Run(handled);
}

// Called when ARC returned a handler list for the |url|.
void OnUrlHandlerList(
    base::WeakPtr<WebContents> web_contents,
    const GURL& url,
    const std::optional<url::Origin>& initiating_origin,
    bool safe_to_bypass_ui,
    std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate,
    base::OnceCallback<void(bool)> handled_cb,
    std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers) {
  // ArcIntentHelperMojoDelegate is already varified non-null.
  DCHECK(mojo_delegate);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // We only reach here if Chrome doesn't think it can handle the URL. If ARC is
  // not running anymore, or Chrome is the only candidate returned, show the
  // usual Chrome OS dialog that says we cannot handle the URL.
  if (!mojo_delegate->IsArcAvailable() ||
      !ArcIconCacheDelegate::GetInstance() || handlers.empty() ||
      IsChromeOnlyAppCandidate(handlers)) {
    ShowExternalProtocolDialogWithoutApps(web_contents, url, initiating_origin,
                                          std::move(mojo_delegate),
                                          std::move(handled_cb));
    return;
  }

  // Check if the |url| should be handled right away without showing the UI.
  GetActionResult result;
  if (HandleUrl(web_contents, url, handlers, handlers.size(), &result,
                safe_to_bypass_ui, mojo_delegate.get())) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto* context = web_contents ? web_contents->GetBrowserContext() : nullptr;

    if (context && result == GetActionResult::HANDLE_URL_IN_ARC) {
      apps::IntentHandlingMetrics::RecordExternalProtocolUserInteractionMetrics(
          context, apps::PickerEntryType::kArc,
          apps::IntentPickerCloseReason::PREFERRED_APP_FOUND,
          /*should_persist=*/false);
    }
#endif
    return std::move(handled_cb).Run(/*handled=*/true);
  }

  // Otherwise, retrieve icons of the activities. Since this function is for
  // handling external protocols, Chrome is rarely in the list, but if the |url|
  // is intent: with fallback or geo:, for example, it may be. In this case, we
  // remove it from the handler list and show the "Stay in Chrome" button
  // instead.
  bool show_stay_in_chrome = false;
  std::vector<ArcIntentHelperMojoDelegate::ActivityName> activities;
  auto it = handlers.begin();
  while (it != handlers.end()) {
    if (it->package_name == kArcIntentHelperPackageName) {
      it = handlers.erase(it);
      show_stay_in_chrome = true;
    } else {
      activities.emplace_back(it->package_name, it->activity_name);
      ++it;
    }
  }
  ArcIconCacheDelegate::GetInstance()->GetActivityIcons(
      activities, base::BindOnce(OnAppIconsReceived, web_contents, url,
                                 initiating_origin, safe_to_bypass_ui,
                                 std::move(handlers), std::move(mojo_delegate),
                                 std::move(handled_cb), show_stay_in_chrome));
}

}  // namespace

void RunArcExternalProtocolDialog(
    const GURL& url,
    const std::optional<url::Origin>& initiating_origin,
    base::WeakPtr<WebContents> web_contents,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    bool is_in_fenced_frame_tree,
    std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate,
    base::OnceCallback<void(bool)> handled_cb) {
  // This function is for external protocols that Chrome cannot handle.
  DCHECK(!url.SchemeIsHTTPOrHTTPS()) << url;

  // For external protocol navigation, always ignore the FROM_API qualifier.
  const ui::PageTransition masked_page_transition =
      apps::LinkCapturingNavigationThrottle::MaskOutPageTransition(
          page_transition, ui::PAGE_TRANSITION_FROM_API);

  if (!apps::LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
          masked_page_transition,
          /*allow_form_submit=*/true, is_in_fenced_frame_tree,
          has_user_gesture)) {
    LOG(WARNING) << "RunArcExternalProtocolDialog: ignoring " << url
                 << " with PageTransition=" << masked_page_transition
                 << ", is_in_fenced_frame_tree=" << is_in_fenced_frame_tree
                 << ", has_user_gesture=" << has_user_gesture;
    return std::move(handled_cb).Run(false);
  }

  // Check ArcIntentHelperMojoDelegate is set.
  if (!mojo_delegate) {
    LOG(ERROR) << "ArcIntentHelperMojoDelegate is null. "
               << "This is required for mojo connection ."
               << "For testing, set FakeArcIntentHelperMojo.";
    return std::move(handled_cb).Run(false);
  }

  // Make sure that RequestUrlHandlerList API is supported before resetting
  // user data.
  if (!mojo_delegate->IsRequestUrlHandlerListAvailable()) {
    // RequestUrlHandlerList is either not supported or not yet ready.
    ShowExternalProtocolDialogWithoutApps(web_contents, url, initiating_origin,
                                          std::move(mojo_delegate),
                                          std::move(handled_cb));
    return;
  }

  if (!web_contents || !web_contents->GetBrowserContext() ||
      web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return std::move(handled_cb).Run(/*handled=*/false);
  }

  const bool safe_to_bypass_ui =
      GetAndResetSafeToRedirectToArcWithoutUserConfirmationFlag(
          web_contents.get());

  // Show ARC version of the dialog, which is IntentPickerBubbleView. To show
  // the bubble view, we need to ask ARC for a handler list first.
  mojo_delegate->RequestUrlHandlerList(
      url.spec(),
      base::BindOnce(OnUrlHandlerList, web_contents, url, initiating_origin,
                     safe_to_bypass_ui, std::move(mojo_delegate),
                     std::move(handled_cb)));
}

GetActionResult GetActionForTesting(
    const GURL& original_url,
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>& handlers,
    size_t selected_app_index,
    GurlAndActivityInfo* out_url_and_activity_name,
    bool* safe_to_bypass_ui) {
  return GetAction(original_url, handlers, selected_app_index,
                   out_url_and_activity_name, safe_to_bypass_ui);
}

GURL GetUrlToNavigateOnDeactivateForTesting(
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>&
        handlers) {
  return GetUrlToNavigateOnDeactivate(handlers);
}

bool GetAndResetSafeToRedirectToArcWithoutUserConfirmationFlagForTesting(
    WebContents* web_contents) {
  return GetAndResetSafeToRedirectToArcWithoutUserConfirmationFlag(
      web_contents);
}

bool IsChromeAnAppCandidateForTesting(
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>&
        handlers) {
  return IsChromeAnAppCandidate(handlers);
}

void OnIntentPickerClosedForTesting(
    base::WeakPtr<WebContents> web_contents,
    const GURL& url,
    bool safe_to_bypass_ui,
    std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers,
    std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate,
    std::vector<SharingTargetDeviceInfo> devices,
    const std::string& selected_app_package,
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason reason,
    bool should_persist) {
  OnIntentPickerClosed(
      web_contents, url, safe_to_bypass_ui, std::move(handlers),
      std::move(mojo_delegate), std::move(devices),
      apps::IntentPickerBubbleType::kExternalProtocol, selected_app_package,
      entry_type, reason, should_persist);
}

}  // namespace arc
