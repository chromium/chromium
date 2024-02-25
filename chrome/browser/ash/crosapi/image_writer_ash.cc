// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/image_writer_ash.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/unguessable_token.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/image_writer_private.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "url/gurl.h"

namespace crosapi {

namespace {

crosapi::mojom::RemovableStorageDevicePtr ToMojo(
    const extensions::api::image_writer_private::RemovableStorageDevice&
        device) {
  auto mojo_device = crosapi::mojom::RemovableStorageDevice::New();
  mojo_device->storage_unit_id = device.storage_unit_id;
  mojo_device->capacity = device.capacity;
  mojo_device->vendor = device.vendor;
  mojo_device->model = device.model;
  mojo_device->removable = device.removable;
  return mojo_device;
}

content::BrowserContext* GetActiveUserBrowserContext() {
  return ProfileManager::GetActiveUserProfile();
}

}  // namespace

ImageWriterAsh::ImageWriterAsh() = default;

ImageWriterAsh::~ImageWriterAsh() = default;

void ImageWriterAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ImageWriter> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void ImageWriterAsh::ListRemovableStorageDevices(
    ListRemovableStorageDevicesCallback callback) {
  extensions::RemovableStorageProvider::GetAllDevices(
      base::BindOnce(&ImageWriterAsh::OnDeviceListReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ImageWriterAsh::DestroyPartitions(
    const std::string& storage_unit_id,
    mojo::PendingRemote<mojom::ImageWriterClient> remote_client,
    DestroyPartitionsCallback callback) {
  mojo::Remote<mojom::ImageWriterClient> remote(std::move(remote_client));
  base::UnguessableToken remote_client_token = base::UnguessableToken::Create();
  remote.set_disconnect_handler(
      base::BindOnce(&ImageWriterAsh::OnImageWriterClientDisconnected,
                     weak_ptr_factory_.GetWeakPtr(), remote_client_token));
  remote_image_writer_clients_.emplace(remote_client_token.ToString(),
                                       std::move(remote));

  // Pass the string of |remote_client_token| to OperationManager, which will
  // be used to dispatch writing progress events back.
  extensions::image_writer::OperationManager::Get(GetActiveUserBrowserContext())
      ->DestroyPartitions(
          /*extension_id=*/remote_client_token.ToString(), storage_unit_id,
          base::BindOnce(&ImageWriterAsh::OnOperationCompleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ImageWriterAsh::WriteFromUrl(
    const std::string& storage_unit_id,
    const GURL& image_url,
    const std::optional<std::string>& image_hash,
    mojo::PendingRemote<mojom::ImageWriterClient> remote_client,
    WriteFromUrlCallback callback) {
  mojo::Remote<mojom::ImageWriterClient> remote(std::move(remote_client));
  base::UnguessableToken remote_client_token = base::UnguessableToken::Create();
  remote.set_disconnect_handler(
      base::BindOnce(&ImageWriterAsh::OnImageWriterClientDisconnected,
                     weak_ptr_factory_.GetWeakPtr(), remote_client_token));
  remote_image_writer_clients_.emplace(remote_client_token.ToString(),
                                       std::move(remote));

  extensions::image_writer::OperationManager::Get(GetActiveUserBrowserContext())
      ->StartWriteFromUrl(
          /*extension_id=*/remote_client_token.ToString(), image_url,
          image_hash ? image_hash.value() : "", storage_unit_id,
          base::BindOnce(&ImageWriterAsh::OnOperationCompleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ImageWriterAsh::WriteFromFile(
    const std::string& storage_unit_id,
    const base::FilePath& image_path,
    mojo::PendingRemote<mojom::ImageWriterClient> remote_client,
    WriteFromFileCallback callback) {
  mojo::Remote<mojom::ImageWriterClient> remote(std::move(remote_client));
  base::UnguessableToken remote_client_token = base::UnguessableToken::Create();
  remote.set_disconnect_handler(
      base::BindOnce(&ImageWriterAsh::OnImageWriterClientDisconnected,
                     weak_ptr_factory_.GetWeakPtr(), remote_client_token));
  remote_image_writer_clients_.emplace(remote_client_token.ToString(),
                                       std::move(remote));

  extensions::image_writer::OperationManager::Get(GetActiveUserBrowserContext())
      ->StartWriteFromFile(
          /*extension_id=*/remote_client_token.ToString(), image_path,
          storage_unit_id,
          base::BindOnce(&ImageWriterAsh::OnOperationCompleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ImageWriterAsh::DispatchOnWriteProgressEvent(
    const std::string& client_token_string,
    crosapi::mojom::Stage stage,
    uint32_t percent_complete) {
  auto it = remote_image_writer_clients_.find(client_token_string);
  if (it != remote_image_writer_clients_.end())
    it->second->DispatchOnWriteProgressEvent(stage, percent_complete);
}

void ImageWriterAsh::DispatchOnWriteCompleteEvent(
    const std::string& client_token_string) {
  auto it = remote_image_writer_clients_.find(client_token_string);
  if (it != remote_image_writer_clients_.end()) {
    it->second->DispatchOnWriteCompleteEvent();
    // Clean up the remote client after the writing operation completes.
    remote_image_writer_clients_.erase(it);
  }
}

void ImageWriterAsh::DispatchOnWriteErrorEvent(
    const std::string& client_token_string,
    crosapi::mojom::Stage stage,
    uint32_t percent_complete,
    const std::string& error) {
  auto it = remote_image_writer_clients_.find(client_token_string);
  if (it != remote_image_writer_clients_.end()) {
    it->second->DispatchOnWriteErrorEvent(stage, percent_complete, error);
    // Clean up the remote client after the writing operation fails.
    remote_image_writer_clients_.erase(it);
  }
}

bool ImageWriterAsh::IsRemoteClientToken(const std::string& id) const {
  return base::Contains(remote_image_writer_clients_, id);
}

void ImageWriterAsh::OnImageWriterClientDisconnected(
    const base::UnguessableToken& remote_client_token) {
  auto it = remote_image_writer_clients_.find(remote_client_token.ToString());
  if (it != remote_image_writer_clients_.end()) {
    // Cancel the write operation if there is any pending.
    extensions::image_writer::OperationManager::Get(
        GetActiveUserBrowserContext())
        ->CancelWrite(/*extension_id=*/remote_client_token.ToString(),
                      base::BindOnce(&ImageWriterAsh::OnCancelWriteDone,
                                     weak_ptr_factory_.GetWeakPtr()));

    remote_image_writer_clients_.erase(it);
  }
}

void ImageWriterAsh::OnDeviceListReady(
    ListRemovableStorageDevicesCallback callback,
    scoped_refptr<StorageDeviceList> device_list) {
  if (!device_list) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::vector<crosapi::mojom::RemovableStorageDevicePtr> mojo_devices;
  for (const auto& device : device_list->data) {
    mojo_devices.push_back(ToMojo(device));
  }

  std::move(callback).Run(std::move(mojo_devices));
}

void ImageWriterAsh::OnOperationCompleted(OperationCallback callback,
                                          bool success,
                                          const std::string& error) {
  std::move(callback).Run(success ? std::nullopt : std::make_optional(error));
}

void ImageWriterAsh::OnCancelWriteDone(bool success, const std::string& error) {
  if (!success)
    DLOG(WARNING) << "Failed to cancel write for remote client: " << error;
}

}  // namespace crosapi
