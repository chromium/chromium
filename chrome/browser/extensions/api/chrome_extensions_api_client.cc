// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/automation_internal/chrome_automation_internal_api_delegate.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"
#include "chrome/browser/extensions/api/declarative_content/default_content_predicate_evaluators.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/feedback_private/chrome_feedback_private_delegate.h"
#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate.h"
#include "chrome/browser/extensions/api/file_system/consent_provider_impl.h"
#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"
#include "chrome/browser/extensions/api/messaging/chrome_messaging_delegate.h"
#include "chrome/browser/extensions/api/metrics_private/chrome_metrics_private_delegate.h"
#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"
#include "chrome/browser/extensions/api/storage/sync_value_store_cache.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/system_display/display_info_provider.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/guest_view/app_view/chrome_app_view_guest_delegate.h"
#include "chrome/browser/guest_view/chrome_guest_view_manager_delegate.h"
#include "chrome/browser/guest_view/extension_options/chrome_extension_options_guest_delegate.h"
#include "chrome/browser/guest_view/mime_handler_view/chrome_mime_handler_view_guest_delegate.h"
#include "chrome/browser/guest_view/web_view/chrome_web_view_guest_delegate.h"
#include "chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/webui/devtools/devtools_ui.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/value_store/value_store_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "google_apis/gaia/gaia_urls.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/extensions/api/file_handlers/non_native_file_system_delegate_chromeos.h"
#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate_ash.h"
#include "chrome/browser/extensions/api/media_perception_private/media_perception_api_delegate_chromeos.h"
#include "chrome/browser/extensions/api/virtual_keyboard_private/chrome_virtual_keyboard_delegate.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate_lacros.h"
#include "chrome/browser/extensions/api/virtual_keyboard_private/lacros_virtual_keyboard_delegate.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/extensions/clipboard_extension_helper_chromeos.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif

namespace extensions {

ChromeExtensionsAPIClient::ChromeExtensionsAPIClient() = default;

ChromeExtensionsAPIClient::~ChromeExtensionsAPIClient() {}

void ChromeExtensionsAPIClient::AddAdditionalValueStoreCaches(
    content::BrowserContext* context,
    const scoped_refptr<value_store::ValueStoreFactory>& factory,
    SettingsChangedCallback observer,
    std::map<settings_namespace::Namespace,
             raw_ptr<ValueStoreCache, CtnExperimental>>* caches) {
  // Add support for chrome.storage.sync.
  (*caches)[settings_namespace::SYNC] =
      new SyncValueStoreCache(factory, observer, context->GetPath());

  // Add support for chrome.storage.managed.
  (*caches)[settings_namespace::MANAGED] = new ManagedValueStoreCache(
      *Profile::FromBrowserContext(context), factory, observer);
}

void ChromeExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrintingForWebContents(web_contents);
#endif
}

bool ChromeExtensionsAPIClient::ShouldHideResponseHeader(
    const GURL& url,
    const std::string& header_name) const {
  // Gaia may send a OAUth2 authorization code in the Dice response header,
  // which could allow an extension to generate a refresh token for the account.
  return (
      (url.host_piece() == GaiaUrls::GetInstance()->gaia_url().host_piece()) &&
      (base::CompareCaseInsensitiveASCII(header_name,
                                         signin::kDiceResponseHeader) == 0));
}

bool ChromeExtensionsAPIClient::ShouldHideBrowserNetworkRequest(
    content::BrowserContext* context,
    const WebRequestInfo& request) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Note: browser initiated non-navigation requests are hidden from extensions.
  // But we do still need to protect some sensitive sub-frame navigation
  // requests.
  // Exclude main frame navigation requests.
  bool is_browser_request =
      request.render_process_id == -1 &&
      request.web_request_type != WebRequestResourceType::MAIN_FRAME;

  // Hide requests made by the Devtools frontend.
  bool is_sensitive_request =
      is_browser_request && DevToolsUI::IsFrontendResourceURL(request.url);

  // Hide requests made by the browser on behalf of the NTP.
  is_sensitive_request |=
      (is_browser_request &&
       request.initiator ==
           url::Origin::Create(GURL(chrome::kChromeUINewTabURL)));

  // Hide requests made by the browser on behalf of the 1P WebUI NTP.
  is_sensitive_request |=
      (is_browser_request &&
       request.initiator ==
           url::Origin::Create(GURL(chrome::kChromeUINewTabPageURL)));

  // Hide requests made by the NTP Instant renderer.
  auto* instant_service =
      context
          ? InstantServiceFactory::GetForProfile(static_cast<Profile*>(context))
          : nullptr;
  if (instant_service) {
    is_sensitive_request |=
        instant_service->IsInstantProcess(request.render_process_id);
  }

  return is_sensitive_request;
}

void ChromeExtensionsAPIClient::NotifyWebRequestWithheld(
    int render_process_id,
    int render_frame_id,
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Track down the ExtensionActionRunner and the extension. Since this is
  // asynchronous, we could hit a null anywhere along the path.
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host) {
    return;
  }
  // We don't count subframes and prerendering blocked actions as yet, since
  // there's no way to surface this to the user. Ignore these (which is also
  // what we do for content scripts).
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;
  extensions::ExtensionActionRunner* runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  if (!runner)
    return;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(web_contents->GetBrowserContext())
          ->enabled_extensions()
          .GetByID(extension_id);
  if (!extension)
    return;

  // If the extension doesn't request access to the tab, return. The user
  // invoking the extension on a site grants access to the tab's origin if
  // and only if the extension requested it; without requesting the tab,
  // clicking on the extension won't grant access to the resource.
  // https://crbug.com/891586.
  // TODO(https://157736): We can remove this if extensions require host
  // permissions to the initiator, since then we'll never get into this type
  // of circumstance (the request would be blocked, rather than withheld).
  if (!extension->permissions_data()
           ->withheld_permissions()
           .explicit_hosts()
           .MatchesURL(render_frame_host->GetLastCommittedURL())) {
    return;
  }

  runner->OnWebRequestBlocked(extension);
}

void ChromeExtensionsAPIClient::UpdateActionCount(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    int tab_id,
    int action_count,
    bool clear_badge_text) {
  const Extension* extension =
      ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
          extension_id);
  DCHECK(extension);

  ExtensionAction* action =
      ExtensionActionManager::Get(context)->GetExtensionAction(*extension);
  DCHECK(action);

  action->SetDNRActionCount(tab_id, action_count);

  // The badge text should be cleared if |action| contains explicitly set badge
  // text for the |tab_id| when the preference is then toggled on. In this case,
  // the matched action count should take precedence over the badge text.
  if (clear_badge_text)
    action->ClearBadgeText(tab_id);

  content::WebContents* tab_contents = nullptr;
  if (ExtensionTabUtil::GetTabById(
          tab_id, context, true /* include_incognito */, &tab_contents) &&
      tab_contents) {
    ExtensionActionAPI::Get(context)->NotifyChange(action, tab_contents,
                                                   context);
  }
}

void ChromeExtensionsAPIClient::ClearActionCount(
    content::BrowserContext* context,
    const Extension& extension) {
  ExtensionAction* action =
      ExtensionActionManager::Get(context)->GetExtensionAction(extension);
  DCHECK(action);

  action->ClearDNRActionCountForAllTabs();

  std::vector<content::WebContents*> contents_to_notify =
      ExtensionTabUtil::GetAllActiveWebContentsForContext(
          context, true /* include_incognito */);

  for (auto* active_contents : contents_to_notify) {
    ExtensionActionAPI::Get(context)->NotifyChange(action, active_contents,
                                                   context);
  }
}

void ChromeExtensionsAPIClient::OpenFileUrl(
    const GURL& file_url,
    content::BrowserContext* browser_context) {
  CHECK(file_url.is_valid());
  CHECK(file_url.SchemeIsFile());
  Profile* profile = Profile::FromBrowserContext(browser_context);
  NavigateParams navigate_params(profile, file_url,
                                 ui::PAGE_TRANSITION_FROM_API);
  navigate_params.disposition = WindowOpenDisposition::CURRENT_TAB;
  navigate_params.browser =
      chrome::FindTabbedBrowser(profile, /*match_original_profiles=*/false);
  Navigate(&navigate_params);
}

AppViewGuestDelegate* ChromeExtensionsAPIClient::CreateAppViewGuestDelegate()
    const {
  return new ChromeAppViewGuestDelegate();
}

ExtensionOptionsGuestDelegate*
ChromeExtensionsAPIClient::CreateExtensionOptionsGuestDelegate(
    ExtensionOptionsGuest* guest) const {
  return new ChromeExtensionOptionsGuestDelegate(guest);
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
ChromeExtensionsAPIClient::CreateGuestViewManagerDelegate() const {
  return std::make_unique<ChromeGuestViewManagerDelegate>();
}

std::unique_ptr<MimeHandlerViewGuestDelegate>
ChromeExtensionsAPIClient::CreateMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest) const {
  return std::make_unique<ChromeMimeHandlerViewGuestDelegate>();
}

WebViewGuestDelegate* ChromeExtensionsAPIClient::CreateWebViewGuestDelegate(
    WebViewGuest* web_view_guest) const {
  return new ChromeWebViewGuestDelegate(web_view_guest);
}

WebViewPermissionHelperDelegate*
ChromeExtensionsAPIClient::CreateWebViewPermissionHelperDelegate(
    WebViewPermissionHelper* web_view_permission_helper) const {
  return new ChromeWebViewPermissionHelperDelegate(web_view_permission_helper);
}

#if BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<ConsentProvider>
ChromeExtensionsAPIClient::CreateConsentProvider(
    content::BrowserContext* browser_context) const {
  auto consent_provider_delegate =
      std::make_unique<file_system_api::ConsentProviderDelegate>(
          Profile::FromBrowserContext(browser_context));
  return std::make_unique<file_system_api::ConsentProviderImpl>(
      std::move(consent_provider_delegate));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

scoped_refptr<ContentRulesRegistry>
ChromeExtensionsAPIClient::CreateContentRulesRegistry(
    content::BrowserContext* browser_context,
    RulesCacheDelegate* cache_delegate) const {
  return base::MakeRefCounted<ChromeContentRulesRegistry>(
      browser_context, cache_delegate,
      base::BindOnce(&CreateDefaultContentPredicateEvaluators,
                     base::Unretained(browser_context)));
}

std::unique_ptr<DevicePermissionsPrompt>
ChromeExtensionsAPIClient::CreateDevicePermissionsPrompt(
    content::WebContents* web_contents) const {
  return std::make_unique<ChromeDevicePermissionsPrompt>(web_contents);
}

#if BUILDFLAG(IS_CHROMEOS)
bool ChromeExtensionsAPIClient::ShouldAllowDetachingUsb(int vid,
                                                        int pid) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const base::Value::List* policy_list;
  if (ash::CrosSettings::Get()->GetList(ash::kUsbDetachableAllowlist,
                                        &policy_list)) {
    for (const auto& entry : *policy_list) {
      const base::Value::Dict* entry_dict = entry.GetIfDict();
      if (entry_dict &&
          entry_dict->FindInt(ash::kUsbDetachableAllowlistKeyVid) == vid &&
          entry_dict->FindInt(ash::kUsbDetachableAllowlistKeyPid) == pid) {
        return true;
      }
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const crosapi::mojom::DeviceSettings* device_settings =
      chromeos::BrowserParamsProxy::Get()->DeviceSettings().get();
  if (device_settings && device_settings->usb_detachable_allow_list) {
    for (const auto& entry :
         device_settings->usb_detachable_allow_list->usb_device_ids) {
      if (entry->has_vendor_id && entry->vendor_id == vid &&
          entry->has_product_id && entry->product_id == pid) {
        return true;
      }
    }
  }
#endif
  return false;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::unique_ptr<VirtualKeyboardDelegate>
ChromeExtensionsAPIClient::CreateVirtualKeyboardDelegate(
    content::BrowserContext* browser_context) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<ChromeVirtualKeyboardDelegate>(browser_context);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<LacrosVirtualKeyboardDelegate>();
#else
  return nullptr;
#endif
}

ManagementAPIDelegate* ChromeExtensionsAPIClient::CreateManagementAPIDelegate()
    const {
  return new ChromeManagementAPIDelegate;
}

std::unique_ptr<SupervisedUserExtensionsDelegate>
ChromeExtensionsAPIClient::CreateSupervisedUserExtensionsDelegate(
    content::BrowserContext* browser_context) const {
  return std::make_unique<SupervisedUserExtensionsDelegateImpl>(
      browser_context);
}

std::unique_ptr<DisplayInfoProvider>
ChromeExtensionsAPIClient::CreateDisplayInfoProvider() const {
  return CreateChromeDisplayInfoProvider();
}

MetricsPrivateDelegate* ChromeExtensionsAPIClient::GetMetricsPrivateDelegate() {
  if (!metrics_private_delegate_)
    metrics_private_delegate_ =
        std::make_unique<ChromeMetricsPrivateDelegate>();
  return metrics_private_delegate_.get();
}

FileSystemDelegate* ChromeExtensionsAPIClient::GetFileSystemDelegate() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  using ChromeFileSystemDelegate_Use = ChromeFileSystemDelegateAsh;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  using ChromeFileSystemDelegate_Use = ChromeFileSystemDelegateLacros;
#else
  using ChromeFileSystemDelegate_Use = ChromeFileSystemDelegate;
#endif
  if (!file_system_delegate_)
    file_system_delegate_ = std::make_unique<ChromeFileSystemDelegate_Use>();
  return file_system_delegate_.get();
}

MessagingDelegate* ChromeExtensionsAPIClient::GetMessagingDelegate() {
  if (!messaging_delegate_)
    messaging_delegate_ = std::make_unique<ChromeMessagingDelegate>();
  return messaging_delegate_.get();
}

FeedbackPrivateDelegate*
ChromeExtensionsAPIClient::GetFeedbackPrivateDelegate() {
  if (!feedback_private_delegate_) {
    feedback_private_delegate_ =
        std::make_unique<ChromeFeedbackPrivateDelegate>();
  }
  return feedback_private_delegate_.get();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
MediaPerceptionAPIDelegate*
ChromeExtensionsAPIClient::GetMediaPerceptionAPIDelegate() {
  if (!media_perception_api_delegate_) {
    media_perception_api_delegate_ =
        std::make_unique<MediaPerceptionAPIDelegateChromeOS>();
  }
  return media_perception_api_delegate_.get();
}

NonNativeFileSystemDelegate*
ChromeExtensionsAPIClient::GetNonNativeFileSystemDelegate() {
  if (!non_native_file_system_delegate_) {
    non_native_file_system_delegate_ =
        std::make_unique<NonNativeFileSystemDelegateChromeOS>();
  }
  return non_native_file_system_delegate_.get();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
void ChromeExtensionsAPIClient::SaveImageDataToClipboard(
    std::vector<uint8_t> image_data,
    api::clipboard::ImageType type,
    AdditionalDataItemList additional_items,
    base::OnceClosure success_callback,
    base::OnceCallback<void(const std::string&)> error_callback) {
  if (!clipboard_extension_helper_)
    clipboard_extension_helper_ = std::make_unique<ClipboardExtensionHelper>();
  clipboard_extension_helper_->DecodeAndSaveImageData(
      std::move(image_data), type, std::move(additional_items),
      std::move(success_callback), std::move(error_callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

AutomationInternalApiDelegate*
ChromeExtensionsAPIClient::GetAutomationInternalApiDelegate() {
  if (!extensions_automation_api_delegate_) {
    extensions_automation_api_delegate_ =
        std::make_unique<ChromeAutomationInternalApiDelegate>();
  }
  return extensions_automation_api_delegate_.get();
}

std::vector<KeyedServiceBaseFactory*>
ChromeExtensionsAPIClient::GetFactoryDependencies() {
  // clang-format off
  return {
      InstantServiceFactory::GetInstance(),
      SupervisedUserServiceFactory::GetInstance(),
  };
  // clang-format on
}

}  // namespace extensions
