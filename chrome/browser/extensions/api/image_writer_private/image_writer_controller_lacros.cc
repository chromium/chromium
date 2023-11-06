// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/image_writer_controller_lacros.h"

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/common/extensions/api/image_writer_private.h"
#include "chromeos/crosapi/mojom/image_writer.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension_id.h"

namespace image_writer_api = extensions::api::image_writer_private;

namespace extensions {

namespace image_writer {

namespace {

const char kUnsupportedAshVersion[] = "UNSUPPORTED_ASH_VERSION";

image_writer_api::Stage FromMojo(crosapi::mojom::Stage mojo_stage) {
  switch (mojo_stage) {
    case crosapi::mojom::Stage::kConfirmation:
      return image_writer_api::Stage::kConfirmation;
    case crosapi::mojom::Stage::kDownload:
      return image_writer_api::Stage::kDownload;
    case crosapi::mojom::Stage::kVerifyDownload:
      return image_writer_api::Stage::kVerifyDownload;
    case crosapi::mojom::Stage::kUnzip:
      return image_writer_api::Stage::kUnzip;
    case crosapi::mojom::Stage::kWrite:
      return image_writer_api::Stage::kWrite;
    case crosapi::mojom::Stage::kVerifyWrite:
      return image_writer_api::Stage::kVerifyWrite;
    case crosapi::mojom::Stage::kUnknown:
      return image_writer_api::Stage::kUnknown;
  }
}

}  // namespace

// Implements crosapi ImageWriterClient interface which receives and routes
// the events about writing operation from ash to the Lacros extension that
// initiates the removable device writing extension API calls.
class ImageWriterControllerLacros::ImageWriterClientLacros
    : public crosapi::mojom::ImageWriterClient {
 public:
  ImageWriterClientLacros(
      content::BrowserContext* browser_context,
      const std::string& extension_id,
      extensions::image_writer::ImageWriterControllerLacros* controller)
      : extension_id_(extension_id),
        browser_context_(browser_context),
        controller_(controller) {}

  ImageWriterClientLacros(const ImageWriterClientLacros&) = delete;
  ImageWriterClientLacros& operator=(const ImageWriterClientLacros&) = delete;
  ~ImageWriterClientLacros() override = default;

  mojo::PendingRemote<crosapi::mojom::ImageWriterClient>
  BindImageWriterClient() {
    return receiver_.BindNewPipeAndPassRemoteWithVersion();
  }

 private:
  // crosapi::mojom::ImageWriterClient:
  void DispatchOnWriteProgressEvent(crosapi::mojom::Stage stage,
                                    uint32_t percent_complete) override {
    image_writer_api::ProgressInfo info;
    info.stage = FromMojo(stage);
    info.percent_complete = percent_complete;
    auto args = image_writer_api::OnWriteProgress::Create(info);
    auto event = std::make_unique<extensions::Event>(
        extensions::events::IMAGE_WRITER_PRIVATE_ON_WRITE_PROGRESS,
        image_writer_api::OnWriteProgress::kEventName, std::move(args));
    extensions::EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(extension_id_, std::move(event));
  }

  void DispatchOnWriteCompleteEvent() override {
    auto args = image_writer_api::OnWriteComplete::Create();
    auto event = std::make_unique<extensions::Event>(
        extensions::events::IMAGE_WRITER_PRIVATE_ON_WRITE_COMPLETE,
        image_writer_api::OnWriteComplete::kEventName, std::move(args));
    extensions::EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(extension_id_, std::move(event));
    controller_->OnPendingClientWriteCompleted(extension_id_);
    //  Note: |this| is deleted at this point.
  }

  void DispatchOnWriteErrorEvent(crosapi::mojom::Stage stage,
                                 uint32_t percent_complete,
                                 const std::string& error) override {
    DLOG(ERROR) << "ImageWriter error: " << error;

    image_writer_api::ProgressInfo info;
    info.stage = FromMojo(stage);
    info.percent_complete = percent_complete;

    auto args = image_writer_api::OnWriteError::Create(info, error);
    auto event = std::make_unique<extensions::Event>(
        extensions::events::IMAGE_WRITER_PRIVATE_ON_WRITE_ERROR,
        image_writer_api::OnWriteError::kEventName, std::move(args));
    extensions::EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(extension_id_, std::move(event));
    controller_->OnPendingClientWriteError(extension_id_);
    // Note: |this| is deleted at this point.
  }

  const ExtensionId extension_id_;
  // Both pointers of |browser_context_| and |controller_| are guaranteed
  // to be valid for the lifetime of this class, as destruction of either
  // BrowserContext or ImageWriterControllerLacros will result in synchronous
  // destruction of this class.
  const raw_ptr<content::BrowserContext> browser_context_;
  const raw_ptr<extensions::image_writer::ImageWriterControllerLacros>
      controller_;

  mojo::Receiver<crosapi::mojom::ImageWriterClient> receiver_{this};
};

ImageWriterControllerLacros::ImageWriterControllerLacros(
    content::BrowserContext* context)
    : browser_context_(context) {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(browser_context_));
  process_manager_observation_.Observe(
      extensions::ProcessManager::Get(browser_context_));
}

ImageWriterControllerLacros::~ImageWriterControllerLacros() = default;

void ImageWriterControllerLacros::ListRemovableStorageDevices(
    ListRemovableStorageDevicesCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (service->IsAvailable<crosapi::mojom::ImageWriter>()) {
    service->GetRemote<crosapi::mojom::ImageWriter>()
        ->ListRemovableStorageDevices(std::move(callback));
  } else {
    std::move(callback).Run(absl::nullopt);
  }
}

void ImageWriterControllerLacros::DestroyPartitions(
    const std::string& extension_id,
    const std::string& storage_unit_id,
    WriteOperationCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::ImageWriter>()) {
    std::move(callback).Run(kUnsupportedAshVersion);
    return;
  }

  if (base::Contains(pending_clients_, extension_id)) {
    std::move(callback).Run(error::kOperationAlreadyInProgress);
    return;
  }

  auto pending_client = std::make_unique<ImageWriterClientLacros>(
      browser_context_, extension_id, this);
  service->GetRemote<crosapi::mojom::ImageWriter>()->DestroyPartitions(
      storage_unit_id, pending_client->BindImageWriterClient(),
      std::move(callback));
  pending_clients_.emplace(extension_id, std::move(pending_client));
}

void ImageWriterControllerLacros::WriteFromUrl(
    const std::string& extension_id,
    const std::string& storage_unit_id,
    const GURL& image_url,
    const absl::optional<std::string>& image_hash,
    WriteOperationCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::ImageWriter>() ||
      service->GetInterfaceVersion<crosapi::mojom::ImageWriter>() < 1) {
    std::move(callback).Run(kUnsupportedAshVersion);
    return;
  }

  if (base::Contains(pending_clients_, extension_id)) {
    std::move(callback).Run(error::kOperationAlreadyInProgress);
    return;
  }

  auto pending_client = std::make_unique<ImageWriterClientLacros>(
      browser_context_, extension_id, this);
  service->GetRemote<crosapi::mojom::ImageWriter>()->WriteFromUrl(
      storage_unit_id, image_url, image_hash,
      pending_client->BindImageWriterClient(), std::move(callback));
  pending_clients_.emplace(extension_id, std::move(pending_client));
}

void ImageWriterControllerLacros::WriteFromFile(
    const std::string& extension_id,
    const std::string& storage_unit_id,
    const base::FilePath& image_path,
    WriteOperationCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::ImageWriter>() ||
      service->GetInterfaceVersion<crosapi::mojom::ImageWriter>() < 1) {
    std::move(callback).Run(kUnsupportedAshVersion);
    return;
  }

  if (base::Contains(pending_clients_, extension_id)) {
    std::move(callback).Run(error::kOperationAlreadyInProgress);
    return;
  }

  auto pending_client = std::make_unique<ImageWriterClientLacros>(
      browser_context_, extension_id, this);
  service->GetRemote<crosapi::mojom::ImageWriter>()->WriteFromFile(
      storage_unit_id, image_path, pending_client->BindImageWriterClient(),
      std::move(callback));
  pending_clients_.emplace(extension_id, std::move(pending_client));
}

void ImageWriterControllerLacros::CancelWrite(const std::string& extension_id,
                                              WriteOperationCallback callback) {
  if (!base::Contains(pending_clients_, extension_id)) {
    std::move(callback).Run(error::kNoOperationInProgress);
    return;
  }

  // Deleting pending client will trigger its disconnect handler in ash,
  // which will cancel its pending write operation if there is any.
  DeletePendingClient(extension_id);
  std::move(callback).Run(absl::nullopt);
}

void ImageWriterControllerLacros::OnPendingClientWriteCompleted(
    const std::string& extension_id) {
  DeletePendingClient(extension_id);
}

void ImageWriterControllerLacros::OnPendingClientWriteError(
    const std::string& extension_id) {
  DeletePendingClient(extension_id);
}

void ImageWriterControllerLacros::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  DCHECK_EQ(browser_context, browser_context_);
  DeletePendingClient(extension->id());
}

void ImageWriterControllerLacros::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  DCHECK(extension_registry_observation_.IsObservingSource(registry));
  extension_registry_observation_.Reset();
  pending_clients_.clear();
}

void ImageWriterControllerLacros::OnBackgroundHostClose(
    const std::string& extension_id) {
  DeletePendingClient(extension_id);
}

void ImageWriterControllerLacros::OnProcessManagerShutdown(
    extensions::ProcessManager* manager) {
  DCHECK(process_manager_observation_.IsObservingSource(manager));
  process_manager_observation_.Reset();
  pending_clients_.clear();
}

void ImageWriterControllerLacros::OnExtensionProcessTerminated(
    const extensions::Extension* extension) {
  DeletePendingClient(extension->id());
}

void ImageWriterControllerLacros::DeletePendingClient(
    const std::string& extension_id) {
  pending_clients_.erase(extension_id);
}

ImageWriterControllerLacros* ImageWriterControllerLacros::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<ImageWriterControllerLacros>::Get(
      context);
}

BrowserContextKeyedAPIFactory<ImageWriterControllerLacros>*
ImageWriterControllerLacros::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<ImageWriterControllerLacros>>
      instance;
  return instance.get();
}

}  // namespace image_writer

}  // namespace extensions
