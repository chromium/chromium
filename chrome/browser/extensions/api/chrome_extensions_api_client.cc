// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/data_use_measurement/data_use_web_contents_observer.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"
#include "chrome/browser/extensions/api/declarative_content/default_content_predicate_evaluators.h"
#include "chrome/browser/extensions/api/feedback_private/chrome_feedback_private_delegate.h"
#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate.h"
#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"
#include "chrome/browser/extensions/api/messaging/chrome_messaging_delegate.h"
#include "chrome/browser/extensions/api/metrics_private/chrome_metrics_private_delegate.h"
#include "chrome/browser/extensions/api/networking_cast_private/chrome_networking_cast_private_delegate.h"
#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"
#include "chrome/browser/extensions/api/storage/sync_value_store_cache.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/guest_view/app_view/chrome_app_view_guest_delegate.h"
#include "chrome/browser/guest_view/chrome_guest_view_manager_delegate.h"
#include "chrome/browser/guest_view/extension_options/chrome_extension_options_guest_delegate.h"
#include "chrome/browser/guest_view/mime_handler_view/chrome_mime_handler_view_guest_delegate.h"
#include "chrome/browser/guest_view/web_view/chrome_web_view_guest_delegate.h"
#include "chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/value_store/value_store_factory.h"
#include "google_apis/gaia/gaia_urls.h"
#include "printing/buildflags/buildflags.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/api/file_handlers/non_native_file_system_delegate_chromeos.h"
#include "chrome/browser/extensions/api/media_perception_private/media_perception_api_delegate_chromeos.h"
#include "chrome/browser/extensions/api/virtual_keyboard_private/chrome_virtual_keyboard_delegate.h"
#include "chrome/browser/extensions/clipboard_extension_helper_chromeos.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif

namespace extensions {

ChromeExtensionsAPIClient::ChromeExtensionsAPIClient() {}

ChromeExtensionsAPIClient::~ChromeExtensionsAPIClient() {}

void ChromeExtensionsAPIClient::AddAdditionalValueStoreCaches(
    content::BrowserContext* context,
    const scoped_refptr<ValueStoreFactory>& factory,
    const scoped_refptr<base::ObserverListThreadSafe<SettingsObserver>>&
        observers,
    std::map<settings_namespace::Namespace, ValueStoreCache*>* caches) {
  // Add support for chrome.storage.sync.
  (*caches)[settings_namespace::SYNC] =
      new SyncValueStoreCache(factory, observers, context->GetPath());

  // Add support for chrome.storage.managed.
  (*caches)[settings_namespace::MANAGED] =
      new ManagedValueStoreCache(context, factory, observers);
}

void ChromeExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrinting(web_contents);
#endif
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents, std::unique_ptr<pdf::PDFWebContentsHelperClient>(
                        new ChromePDFWebContentsHelperClient()));

  data_use_measurement::DataUseWebContentsObserver::CreateForWebContents(
      web_contents);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
}

bool ChromeExtensionsAPIClient::ShouldHideResponseHeader(
    const GURL& url,
    const std::string& header_name) const {
  // Gaia may send a OAUth2 authorization code in the Dice response header,
  // which could allow an extension to generate a refresh token for the account.
  // TODO(crbug.com/890006): Determine if the code here can be cleaned up
  // since browser initiated non-navigation requests are now hidden from
  // extensions.
  return (
      (url.host_piece() == GaiaUrls::GetInstance()->gaia_url().host_piece()) &&
      (base::CompareCaseInsensitiveASCII(header_name,
                                         signin::kDiceResponseHeader) == 0));
}

bool ChromeExtensionsAPIClient::ShouldHideBrowserNetworkRequest(
    const WebRequestInfo& request) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // TODO(crbug.com/890006): Determine if the code here can be cleaned up
  // since browser initiated non-navigation requests are now hidden from
  // extensions.

  // Exclude main frame navigation requests.
  bool is_browser_request = request.render_process_id == -1 &&
                            request.type != content::RESOURCE_TYPE_MAIN_FRAME;

  // Hide requests made by the Devtools frontend.
  bool is_sensitive_request =
      is_browser_request && DevToolsUI::IsFrontendResourceURL(request.url);

  // Hide requests made by the browser on behalf of the NTP.
  is_sensitive_request |=
      (is_browser_request &&
       request.initiator ==
           url::Origin::Create(GURL(chrome::kChromeUINewTabURL)));

  // Hide requests made by the NTP Instant renderer.
  is_sensitive_request |= InstantIOContext::IsInstantProcess(
      request.resource_context, request.render_process_id);

  return is_sensitive_request;
}

void ChromeExtensionsAPIClient::NotifyWebRequestWithheld(
    int render_process_id,
    int render_frame_id,
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto notify_web_request_withheld_on_ui = [](int render_process_id,
                                              int render_frame_id,
                                              const ExtensionId& extension_id) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Track down the ExtensionActionRunner and the extension. Since this is
    // asynchronous, we could hit a null anywhere along the path.
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromID(render_process_id, render_frame_id);
    if (!rfh)
      return;
    // We don't count subframe blocked actions as yet, since there's no way to
    // surface this to the user. Ignore these (which is also what we do for
    // content scripts).
    if (rfh->GetParent())
      return;
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(rfh);
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
             .MatchesURL(rfh->GetLastCommittedURL())) {
      return;
    }

    runner->OnWebRequestBlocked(extension);
  };

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(std::move(notify_web_request_withheld_on_ui),
                     render_process_id, render_frame_id, extension_id));
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
ChromeExtensionsAPIClient::CreateGuestViewManagerDelegate(
    content::BrowserContext* context) const {
  return std::make_unique<ChromeGuestViewManagerDelegate>(context);
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

WebViewPermissionHelperDelegate* ChromeExtensionsAPIClient::
    CreateWebViewPermissionHelperDelegate(
        WebViewPermissionHelper* web_view_permission_helper) const {
  return new ChromeWebViewPermissionHelperDelegate(web_view_permission_helper);
}

scoped_refptr<ContentRulesRegistry>
ChromeExtensionsAPIClient::CreateContentRulesRegistry(
    content::BrowserContext* browser_context,
    RulesCacheDelegate* cache_delegate) const {
  return scoped_refptr<ContentRulesRegistry>(
      new ChromeContentRulesRegistry(
          browser_context,
          cache_delegate,
          base::Bind(&CreateDefaultContentPredicateEvaluators,
                     base::Unretained(browser_context))));
}

std::unique_ptr<DevicePermissionsPrompt>
ChromeExtensionsAPIClient::CreateDevicePermissionsPrompt(
    content::WebContents* web_contents) const {
  return std::make_unique<ChromeDevicePermissionsPrompt>(web_contents);
}

std::unique_ptr<VirtualKeyboardDelegate>
ChromeExtensionsAPIClient::CreateVirtualKeyboardDelegate(
    content::BrowserContext* browser_context) const {
#if defined(OS_CHROMEOS)
  return std::make_unique<ChromeVirtualKeyboardDelegate>(browser_context);
#else
  return nullptr;
#endif
}

ManagementAPIDelegate* ChromeExtensionsAPIClient::CreateManagementAPIDelegate()
    const {
  return new ChromeManagementAPIDelegate;
}

MetricsPrivateDelegate* ChromeExtensionsAPIClient::GetMetricsPrivateDelegate() {
  if (!metrics_private_delegate_)
    metrics_private_delegate_.reset(new ChromeMetricsPrivateDelegate());
  return metrics_private_delegate_.get();
}

NetworkingCastPrivateDelegate*
ChromeExtensionsAPIClient::GetNetworkingCastPrivateDelegate() {
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_MACOSX)
  if (!networking_cast_private_delegate_)
    networking_cast_private_delegate_ =
        ChromeNetworkingCastPrivateDelegate::Create();
#endif
  return networking_cast_private_delegate_.get();
}

FileSystemDelegate* ChromeExtensionsAPIClient::GetFileSystemDelegate() {
  if (!file_system_delegate_)
    file_system_delegate_ = std::make_unique<ChromeFileSystemDelegate>();
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

#if defined(OS_CHROMEOS)
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

void ChromeExtensionsAPIClient::SaveImageDataToClipboard(
    const std::vector<char>& image_data,
    api::clipboard::ImageType type,
    AdditionalDataItemList additional_items,
    const base::Closure& success_callback,
    const base::Callback<void(const std::string&)>& error_callback) {
  if (!clipboard_extension_helper_)
    clipboard_extension_helper_ = std::make_unique<ClipboardExtensionHelper>();
  clipboard_extension_helper_->DecodeAndSaveImageData(
      image_data, type, std::move(additional_items), success_callback,
      error_callback);
}
#endif

}  // namespace extensions
