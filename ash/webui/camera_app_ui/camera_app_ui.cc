// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/camera_app_ui/camera_app_ui.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/system/camera/camera_app_prefs.h"
#include "ash/webui/camera_app_ui/camera_app_helper_impl.h"
#include "ash/webui/camera_app_ui/resources.h"
#include "ash/webui/camera_app_ui/url_constants.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_camera_app_resources_map.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "media/capture/video/chromeos/camera_app_device_provider_impl.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/aura/window.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

namespace {

BASE_FEATURE(kCCALocalOverride,
             "CCALocalOverride",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FilePath::CharType kCCALocalOverrideDirectoryPath[] =
    FILE_PATH_LITERAL("/etc/camera/cca");

void HandleLocalOverrideRequest(
    const std::string& url,
    content::WebUIDataSource::GotDataCallback callback) {
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     [](const std::string& url,
                        content::WebUIDataSource::GotDataCallback callback) {
                       // The url passed in only contain path and query part.
                       auto parsed_url = GURL(kChromeUICameraAppURL + url);
                       // parsed_url.path() includes the leading "/" but
                       // FilePath::Append only allows relative path.
                       base::FilePath file_path =
                           base::FilePath(kCCALocalOverrideDirectoryPath)
                               .Append(base::TrimString(
                                   parsed_url.path_piece(), "/",
                                   base::TrimPositions::TRIM_LEADING));
                       std::string result;
                       if (base::ReadFileToString(file_path, &result)) {
                         std::move(callback).Run(
                             base::MakeRefCounted<base::RefCountedString>(
                                 std::move(result)));
                       } else {
                         std::move(callback).Run(nullptr);
                       }
                     },
                     url, std::move(callback)));
}

void CreateAndAddCameraAppUIHTMLSource(content::BrowserContext* browser_context,
                                       CameraAppUIDelegate* delegate) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, kChromeUICameraAppHost);

  ash::EnableTrustedTypesCSP(source);

  // Add all settings resources.
  source->AddResourcePaths(
      base::make_span(kAshCameraAppResources, kAshCameraAppResourcesSize));

  delegate->PopulateLoadTimeData(source);

  for (const auto& str : kStringResourceMap) {
    source->AddLocalizedString(str.name, str.id);
  }

  source->UseStringsJs();

  if (base::FeatureList::IsEnabled(kCCALocalOverride)) {
    source->SetRequestFilter(
        base::BindRepeating(CameraAppUIShouldEnableLocalOverride),
        base::BindRepeating(HandleLocalOverrideRequest));
  }

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      std::string("worker-src 'self';"));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameAncestors,
      std::string("frame-ancestors 'self';"));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      std::string("frame-src 'self' ") + kChromeUIUntrustedCameraAppURL + ";");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ObjectSrc,
      std::string("object-src 'self';"));

  // Makes camera app cross-origin-isolated to measure memory usage.
  source->OverrideCrossOriginOpenerPolicy("same-origin");
  source->OverrideCrossOriginEmbedderPolicy("require-corp");
}

void GotSalt(
    const url::Origin& origin,
    const std::string& source_id,
    base::OnceCallback<void(const std::optional<std::string>&)> callback,
    const std::string& salt) {
  auto callback_on_io_thread = base::BindOnce(
      [](const std::string& salt, const url::Origin& origin,
         const std::string& source_id,
         base::OnceCallback<void(const std::optional<std::string>&)> callback) {
        content::GetMediaDeviceIDForHMAC(
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, salt,
            std::move(origin), source_id, content::GetIOThreadTaskRunner({}),
            std::move(callback));
      },
      salt, std::move(origin), source_id, std::move(callback));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, std::move(callback_on_io_thread));
}

// Translates the renderer-side source ID to video device id.
void TranslateVideoDeviceId(
    content::BrowserContext* browser_context,
    media_device_salt::MediaDeviceSaltService* salt_service,
    const url::Origin& origin,
    const std::string& source_id,
    base::OnceCallback<void(const std::optional<std::string>&)> callback) {
  if (salt_service) {
    salt_service->GetSalt(
        blink::StorageKey::CreateFirstParty(origin),
        base::BindOnce(&GotSalt, origin, source_id, std::move(callback)));
  } else {
    // If the embedder does not provide a salt service, use the browser
    // context's unique ID as salt.
    GotSalt(origin, source_id, std::move(callback),
            browser_context->UniqueId());
  }
}

void HandleCameraResult(
    content::BrowserContext* context,
    uint32_t intent_id,
    arc::mojom::CameraIntentAction action,
    const std::vector<uint8_t>& data,
    camera_app::mojom::CameraAppHelper::HandleCameraResultCallback callback) {
  auto* intent_helper =
      arc::ArcIntentHelperBridge::GetForBrowserContext(context);
  intent_helper->HandleCameraResult(intent_id, action, data,
                                    std::move(callback));
}

void SendNewCaptureBroadcast(content::BrowserContext* context,
                             bool is_video,
                             std::string file_path) {
  auto* intent_helper =
      arc::ArcIntentHelperBridge::GetForBrowserContext(context);
  intent_helper->SendNewCaptureBroadcast(is_video, file_path);
}

std::unique_ptr<media::CameraAppDeviceProviderImpl>
CreateCameraAppDeviceProvider(
    content::BrowserContext* browser_context,
    media_device_salt::MediaDeviceSaltService* salt_service,
    const url::Origin& security_origin) {
  auto connect_to_bridge_callback = base::BindRepeating(
      [](mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge>
             device_bridge_receiver) {
        // Connects to CameraAppDeviceBridge from video_capture service.
        content::GetVideoCaptureService().ConnectToCameraAppDeviceBridge(
            std::move(device_bridge_receiver));
      });
  auto mapping_callback =
      base::BindRepeating(&TranslateVideoDeviceId, browser_context,
                          salt_service, std::move(security_origin));

  return std::make_unique<media::CameraAppDeviceProviderImpl>(
      std::move(connect_to_bridge_callback), std::move(mapping_callback));
}

std::unique_ptr<CameraAppHelperImpl> CreateCameraAppHelper(
    CameraAppUI* camera_app_ui,
    content::BrowserContext* browser_context,
    aura::Window* window) {
  DCHECK_NE(window, nullptr);
  auto handle_result_callback =
      base::BindRepeating(&HandleCameraResult, browser_context);
  auto send_broadcast_callback =
      base::BindRepeating(&SendNewCaptureBroadcast, browser_context);

  return std::make_unique<CameraAppHelperImpl>(
      camera_app_ui, std::move(handle_result_callback),
      std::move(send_broadcast_callback), window);
}

}  // namespace

bool CameraAppUIShouldEnableLocalOverride(const std::string& url) {
  // Only override files that are copied locally with cca.py deploy.
  if (!(base::StartsWith(url, "js/") || base::StartsWith(url, "css/") ||
        base::StartsWith(url, "images/") || base::StartsWith(url, "views/") ||
        base::StartsWith(url, "sounds/"))) {
    return false;
  }
  // This file is written by `cca.py deploy` and contains version
  // information of deployed file.
  base::FilePath version_path = base::FilePath(kCCALocalOverrideDirectoryPath)
                                    .Append("js/deployed_version.js");
  base::ScopedAllowBlocking allow_blocking;
  if (!base::PathExists(version_path)) {
    return false;
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// CameraAppUI
//
///////////////////////////////////////////////////////////////////////////////

CameraAppUI::CameraAppUI(content::WebUI* web_ui,
                         std::unique_ptr<CameraAppUIDelegate> delegate)
    : ui::MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  // Register auto-granted permissions.
  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin host_origin =
      url::Origin::Create(GURL(kChromeUICameraAppURL));
  allowlist->RegisterAutoGrantedPermission(
      host_origin, ContentSettingsType::MEDIASTREAM_MIC);
  allowlist->RegisterAutoGrantedPermission(
      host_origin, ContentSettingsType::MEDIASTREAM_CAMERA);
  allowlist->RegisterAutoGrantedPermission(
      host_origin, ContentSettingsType::CAMERA_PAN_TILT_ZOOM);
  allowlist->RegisterAutoGrantedPermission(
      host_origin, ContentSettingsType::FILE_SYSTEM_READ_GUARD);
  allowlist->RegisterAutoGrantedPermission(
      host_origin, ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
  allowlist->RegisterAutoGrantedPermission(host_origin,
                                           ContentSettingsType::COOKIES);
  allowlist->RegisterAutoGrantedPermission(host_origin,
                                           ContentSettingsType::IDLE_DETECTION);

  delegate_->SetLaunchDirectory();

  window()->SetProperty(kMinimizeOnBackKey, false);

  // Set up the data source.
  CreateAndAddCameraAppUIHTMLSource(browser_context, delegate_.get());

  // Add ability to request chrome-untrusted: URLs
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  if (camera_app_prefs::ShouldDevToolsOpen()) {
    delegate_->OpenDevToolsWindow(web_ui->GetWebContents());
  }

  content::DevToolsAgentHost::AddObserver(this);
}

CameraAppUI::~CameraAppUI() {
  content::DevToolsAgentHost::RemoveObserver(this);
}

void CameraAppUI::BindInterface(
    mojo::PendingReceiver<cros::mojom::CameraAppDeviceProvider> receiver) {
  content::BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  provider_ = CreateCameraAppDeviceProvider(
      browser_context, delegate_->GetMediaDeviceSaltService(browser_context),
      url::Origin::Create(GURL(kChromeUICameraAppURL)));
  provider_->Bind(std::move(receiver));
}

void CameraAppUI::BindInterface(
    mojo::PendingReceiver<camera_app::mojom::CameraAppHelper> receiver) {
  helper_ = CreateCameraAppHelper(
      this, web_ui()->GetWebContents()->GetBrowserContext(), window());
  helper_->Bind(std::move(receiver));
}

void CameraAppUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window());
  if (widget) {
    // Camera app is always dark.
    widget->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kDark);
  } else {
    LOG(ERROR) << "Can't find widget for CCA window.";
  }

  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

aura::Window* CameraAppUI::window() {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}

const GURL& CameraAppUI::url() {
  return web_ui()->GetWebContents()->GetLastCommittedURL();
}

void CameraAppUI::DevToolsAgentHostAttached(
    content::DevToolsAgentHost* agent_host) {
  if (agent_host->GetWebContents() == nullptr ||
      !base::StartsWith(
          agent_host->GetWebContents()->GetLastCommittedURL().spec(),
          kChromeUICameraAppMainURL)) {
    return;
  }
  camera_app_prefs::SetDevToolsOpenState(true);
}

void CameraAppUI::DevToolsAgentHostDetached(
    content::DevToolsAgentHost* agent_host) {
  if (agent_host->GetWebContents() == nullptr ||
      !base::StartsWith(
          agent_host->GetWebContents()->GetLastCommittedURL().spec(),
          kChromeUICameraAppMainURL)) {
    return;
  }
  camera_app_prefs::SetDevToolsOpenState(false);
}

bool CameraAppUI::IsJavascriptErrorReportingEnabled() {
  // Since we proactively call CrashReportPrivate.reportError() in CCA now.
  return false;
}

WEB_UI_CONTROLLER_TYPE_IMPL(CameraAppUI)

}  // namespace ash
