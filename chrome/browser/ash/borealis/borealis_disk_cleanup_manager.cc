// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_disk_cleanup_manager.h"

#include <cstdint>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/byte_size.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/message_center.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace borealis {

namespace {
constexpr char kBorealisVmName[] = "borealis";
constexpr char kBorealisContainerName[] = "penguin";
constexpr char kBorealisDlcName[] = "borealis-dlc";
constexpr char kHasAllocatedDiskHistogram[] = "Borealis.HasAllocatedDisk";
}  // namespace

BorealisDiskCleanupManager::BorealisDiskCleanupManager(
    std::string cryptohome_id,
    PrefService* pref_service,
    guest_os::GuestOsRegistryService* registry_service)
    : cryptohome_id_(std::move(cryptohome_id)),
      pref_service_(pref_service),
      registry_service_(registry_service) {
  CHECK(pref_service_);
  CHECK(registry_service_);
}

BorealisDiskCleanupManager::~BorealisDiskCleanupManager() = default;

void BorealisDiskCleanupManager::CheckDiskSpace(base::OnceClosure callback) {
  CHECK(!cryptohome_id_.empty());

  auto* concierge_client = ash::ConciergeClient::Get();
  CHECK(concierge_client);

  vm_tools::concierge::ListVmDisksRequest request;
  request.set_cryptohome_id(cryptohome_id_);
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  request.set_vm_name(kBorealisVmName);
  concierge_client->ListVmDisks(
      std::move(request),
      base::BindOnce(&BorealisDiskCleanupManager::OnListVmDisks,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BorealisDiskCleanupManager::OnListVmDisks(
    base::OnceClosure callback,
    std::optional<vm_tools::concierge::ListVmDisksResponse> response) {
  base::ScopedClosureRunner runner(std::move(callback));
  if (!response) {
    LOG(ERROR) << "Failed to list Borealis VM disks: No D-Bus response.";
    return;
  }
  if (!response->success()) {
    LOG(ERROR) << "Failed to list Borealis VM disks: "
               << response->failure_reason();
    return;
  }

  bool vm_exists = false;
  uint64_t disk_size_bytes = 0;
  for (const auto& image : response->images()) {
    if (image.name() == kBorealisVmName) {
      vm_exists = true;
      disk_size_bytes += image.size();
    }
  }

  if (!vm_exists) {
    return;
  }

  const bool has_allocated_disk = disk_size_bytes > 0;
  base::UmaHistogramBoolean(kHasAllocatedDiskHistogram, has_allocated_disk);

  if (has_allocated_disk) {
    ShowCleanupDialog(disk_size_bytes);
  }
}

void BorealisDiskCleanupManager::ShowCleanupDialog(uint64_t disk_size_bytes) {
  if (dialog_widget_ && !dialog_widget_->IsClosed()) {
    return;
  }

  const std::u16string size_str =
      ui::FormatBytes(base::ByteSize(disk_size_bytes));
  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>())
          .SetTitle(
              l10n_util::GetStringUTF16(IDS_BOREALIS_DISK_CLEANUP_DIALOG_TITLE))
          .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringFUTF16(
              IDS_BOREALIS_DISK_CLEANUP_DIALOG_MESSAGE, size_str)))
          .AddOkButton(
              base::BindOnce(&BorealisDiskCleanupManager::OnFreeDiskSpace,
                             weak_factory_.GetWeakPtr(), disk_size_bytes),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_BOREALIS_DISK_CLEANUP_DIALOG_OK_BUTTON)))
          .AddCancelButton(
              base::DoNothing(),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_BOREALIS_DISK_CLEANUP_DIALOG_CANCEL_BUTTON)))
          .Build();

  auto bubble = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kSystem);
  bubble->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  // A modal bubble has no anchor view, so center it on the primary display.
  // SystemModalContainerLayoutManager keeps it centered afterwards.
  bubble->SetArrow(views::BubbleBorder::Arrow::FLOAT);
  bubble->SetAnchorRect(
      {display::Screen::Get()->GetPrimaryDisplay().work_area().CenterPoint(),
       {}});
  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();
  dialog_widget_.reset(views::DialogDelegate::CreateDialogWidget(
      std::move(bubble), /*context=*/root_window, /*parent=*/nullptr));
  dialog_widget_->Show();
}

void BorealisDiskCleanupManager::OnFreeDiskSpace(uint64_t disk_size_bytes) {
  CHECK(!cryptohome_id_.empty());
  registry_service_->ClearApplicationList(
      vm_tools::apps::BOREALIS, kBorealisVmName, kBorealisContainerName);

  auto* concierge_client = ash::ConciergeClient::Get();
  CHECK(concierge_client);

  vm_tools::concierge::DestroyDiskImageRequest request;
  request.set_cryptohome_id(cryptohome_id_);
  request.set_vm_name(kBorealisVmName);
  concierge_client->DestroyDiskImage(
      std::move(request),
      base::BindOnce(&BorealisDiskCleanupManager::OnDestroyDiskImage,
                     weak_factory_.GetWeakPtr(), disk_size_bytes));
}

void BorealisDiskCleanupManager::OnDestroyDiskImage(
    uint64_t disk_size_bytes,
    std::optional<vm_tools::concierge::DestroyDiskImageResponse> response) {
  if (!response ||
      (response->status() != vm_tools::concierge::DISK_STATUS_DESTROYED &&
       response->status() != vm_tools::concierge::DISK_STATUS_DOES_NOT_EXIST)) {
    LOG(ERROR) << "Failed to destroy Borealis disk image.";
    return;
  }

  auto* dlcservice_client = ash::DlcserviceClient::Get();
  CHECK(dlcservice_client);
  dlcservice_client->Uninstall(
      kBorealisDlcName,
      base::BindOnce(&BorealisDiskCleanupManager::OnDlcUninstalled,
                     weak_factory_.GetWeakPtr(), disk_size_bytes));
}

void BorealisDiskCleanupManager::OnDlcUninstalled(uint64_t disk_size_bytes,
                                                  std::string_view dlc_err) {
  if (dlc_err != dlcservice::kErrorNone) {
    LOG(WARNING) << "Borealis DLC uninstall returned: " << dlc_err;
  }
  pref_service_->SetBoolean(prefs::kBorealisInstalledOnDevice, false);
  ShowCleanupNotification(disk_size_bytes);
}

void BorealisDiskCleanupManager::ShowCleanupNotification(
    uint64_t disk_size_bytes) {
  message_center::MessageCenter::Get()->AddNotification(
      ash::SystemNotificationBuilder()
          .SetId(kCleanupNotificationId)
          .SetCatalogName(ash::NotificationCatalogName::kBorealisContext)
          .SetTitle(
              l10n_util::GetStringUTF16(IDS_BOREALIS_DISK_CLEANUP_DIALOG_TITLE))
          .SetMessage(l10n_util::GetStringFUTF16(
              IDS_BOREALIS_DISK_CLEANUP_NOTIFICATION_MESSAGE,
              ui::FormatBytes(base::ByteSize(disk_size_bytes))))
          .SetSmallImage(ash::kNotificationStorageFullIcon)
          .BuildPtr(/*keep_timestamp=*/false));
}

}  // namespace borealis
