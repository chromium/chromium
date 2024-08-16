// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/screen_capture/arc_screen_capture_bridge.h"

#include <utility>
#include <vector>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/arc/screen_capture/arc_screen_capture_session.h"
#include "chrome/browser/media/webrtc/desktop_media_list_ash.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"

namespace {
constexpr char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
constexpr char kTestImageRelease[] = "testimage";
}  // namespace

namespace arc {
namespace {

// Singleton factory for ArcScreenCaptureBridge
class ArcScreenCaptureBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcScreenCaptureBridge,
          ArcScreenCaptureBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcScreenCaptureBridgeFactory";

  static ArcScreenCaptureBridgeFactory* GetInstance() {
    return base::Singleton<ArcScreenCaptureBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcScreenCaptureBridgeFactory>;
  ArcScreenCaptureBridgeFactory() = default;
  ~ArcScreenCaptureBridgeFactory() override = default;
};

}  // namespace

// static
ArcScreenCaptureBridge* ArcScreenCaptureBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcScreenCaptureBridgeFactory::GetForBrowserContext(context);
}

ArcScreenCaptureBridge::PendingCaptureParams::PendingCaptureParams(
    std::unique_ptr<DesktopMediaPicker> picker,
    const std::string& display_name,
    RequestPermissionCallback callback)
    : picker(std::move(picker)),
      display_name(display_name),
      callback(std::move(callback)) {}

ArcScreenCaptureBridge::PendingCaptureParams::~PendingCaptureParams() {}

ArcScreenCaptureBridge::GrantedCaptureParams::GrantedCaptureParams(
    const std::string& display_name,
    content::DesktopMediaID desktop_id,
    bool enable_notification)
    : display_name(display_name),
      desktop_id(desktop_id),
      enable_notification(enable_notification) {}

ArcScreenCaptureBridge::GrantedCaptureParams::~GrantedCaptureParams() {}

ArcScreenCaptureBridge::ArcScreenCaptureBridge(content::BrowserContext* context,
                                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->screen_capture()->SetHost(this);
}

ArcScreenCaptureBridge::~ArcScreenCaptureBridge() {
  arc_bridge_service_->screen_capture()->SetHost(nullptr);
}

void ArcScreenCaptureBridge::RequestPermission(
    const std::string& display_name,
    const std::string& package_name,
    RequestPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<DesktopMediaPicker> picker =
      DesktopMediaPicker::Create(nullptr);
  std::vector<std::unique_ptr<DesktopMediaList>> source_lists;
  source_lists.emplace_back(
      std::make_unique<DesktopMediaListAsh>(DesktopMediaList::Type::kScreen));
  const std::u16string display_name16 = base::UTF8ToUTF16(display_name);
  DesktopMediaPicker::Params picker_params{
      DesktopMediaPicker::Params::RequestSource::kArcScreenCapture};
  picker_params.context = ash::Shell::GetRootWindowForDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  picker_params.modality = ui::mojom::ModalType::kSystem;
  picker_params.app_name = display_name16;
  picker_params.target_name = display_name16;
  if (pending_permissions_map_.find(package_name) !=
      pending_permissions_map_.end()) {
    LOG(ERROR) << "Screen capture permissions requested while pending request "
                  "was active: "
               << package_name;
    std::move(callback).Run(false);
    return;
  }
  pending_permissions_map_.emplace(
      std::piecewise_construct, std::forward_as_tuple(package_name),
      std::forward_as_tuple(std::move(picker), display_name,
                            std::move(callback)));
  pending_permissions_map_.find(package_name)
      ->second.picker->Show(
          picker_params, std::move(source_lists),
          base::BindOnce(&ArcScreenCaptureBridge::PermissionPromptCallback,
                         base::Unretained(this), package_name));
}

void ArcScreenCaptureBridge::PermissionPromptCallback(
    const std::string& package_name,
    content::DesktopMediaID desktop_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto found = pending_permissions_map_.find(package_name);
  if (found == pending_permissions_map_.end()) {
    // This is normal if the dialog was accepted from testing.
    return;
  }
  if (desktop_id.is_null()) {
    std::move(found->second.callback).Run(false);
    pending_permissions_map_.erase(found);
    return;
  }
  // Remove any existing entry since emplace will not overwrite it.
  // This is OK since these persist forever and this may be requested again with
  // a different desktop.
  granted_permissions_map_.erase(package_name);
  granted_permissions_map_.emplace(
      package_name, GrantedCaptureParams(found->second.display_name, desktop_id,
                                         true /* enable notification */));
  std::move(found->second.callback).Run(true);
  pending_permissions_map_.erase(found);
}

void ArcScreenCaptureBridge::TestModeAcceptPermission(
    const std::string& package_name) {
  std::string track;
  if (!base::SysInfo::GetLsbReleaseValue(kChromeOSReleaseTrack, &track))
    return;
  if (track.find(kTestImageRelease) == std::string::npos)
    return;
  // We are a testimage build, so this call is allowed. To do this, invoke the
  // Mojo callback after taking it from our map. This will prevent it from
  // getting called when we forcibly close the dialog.
  auto found = pending_permissions_map_.find(package_name);
  if (found == pending_permissions_map_.end()) {
    LOG(ERROR) << "Requested to accept dialog for testing, but dialog not "
                  "being shown for "
               << package_name;
    return;
  }
  granted_permissions_map_.erase(package_name);
  granted_permissions_map_.emplace(
      package_name,
      GrantedCaptureParams(found->second.display_name,
                           content::DesktopMediaID::RegisterNativeWindow(
                               content::DesktopMediaID::TYPE_SCREEN,
                               ash::Shell::GetPrimaryRootWindow()),
                           false /* enable notification */));
  std::move(found->second.callback).Run(true);
  pending_permissions_map_.erase(found);
  // The dialog will be closed when 'found' goes out of scope and is
  // destructed and the dialog within it is destructed.

  // If we're auto-sharing the screen in test mode, we don't want to record
  // the cursor, so turn it off.
  ash::Shell::Get()->cursor_manager()->HideCursor();
}

void ArcScreenCaptureBridge::OpenSession(
    mojo::PendingRemote<mojom::ScreenCaptureSessionNotifier> notifier,
    const std::string& package_name,
    const gfx::Size& size,
    OpenSessionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto found = granted_permissions_map_.find(package_name);
  if (found == granted_permissions_map_.end()) {
    LOG(ERROR) << "Attempt to open screen capture session without granted "
                  "permissions for package "
               << package_name;
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  // TODO(crbug.com/41454219): Remove this temporary conversion to InterfacePtr
  // once OpenSession callback from
  // //ash/components/arc/mojom/screen_capture.mojom could take pending_remote
  // directly. Refer to crrev.com/c/1868870.
  mojo::PendingRemote<mojom::ScreenCaptureSession>
      screen_capture_session_remote(ArcScreenCaptureSession::Create(
          std::move(notifier), found->second.display_name,
          found->second.desktop_id, size, found->second.enable_notification));
  std::move(callback).Run(std::move(screen_capture_session_remote));
}

// static
void ArcScreenCaptureBridge::EnsureFactoryBuilt() {
  ArcScreenCaptureBridgeFactory::GetInstance();
}

}  // namespace arc
