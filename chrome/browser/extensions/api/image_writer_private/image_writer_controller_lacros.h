// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_CONTROLLER_LACROS_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/crosapi/mojom/image_writer.mojom.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {

namespace image_writer {

class ImageWriterControllerLacros : public BrowserContextKeyedAPI,
                                    public ExtensionRegistryObserver,
                                    public ProcessManagerObserver {
 public:
  explicit ImageWriterControllerLacros(content::BrowserContext* context);
  ImageWriterControllerLacros(const ImageWriterControllerLacros&) = delete;
  ImageWriterControllerLacros& operator=(const ImageWriterControllerLacros&) =
      delete;
  ~ImageWriterControllerLacros() override;

  using ListRemovableStorageDevicesCallback =
      crosapi::mojom::ImageWriter::ListRemovableStorageDevicesCallback;
  void ListRemovableStorageDevices(
      ListRemovableStorageDevicesCallback callback);

  using WriteOperationCallback =
      base::OnceCallback<void(const absl::optional<std::string>&)>;
  void DestroyPartitions(const std::string& extension_id,
                         const std::string& storage_unit_id,
                         WriteOperationCallback callback);
  void WriteFromUrl(const std::string& extension_id,
                    const std::string& storage_unit_id,
                    const GURL& image_url,
                    const absl::optional<std::string>& image_hash,
                    WriteOperationCallback callback);
  void WriteFromFile(const std::string& extension_id,
                     const std::string& storage_unit_id,
                     const base::FilePath& image_path,
                     WriteOperationCallback callback);
  void CancelWrite(const std::string& extension_id,
                   WriteOperationCallback callback);

  void OnPendingClientWriteCompleted(const std::string& extension_id);
  void OnPendingClientWriteError(const std::string& extension_id);

  // BrowserContextKeyedAPI Implementation.
  static BrowserContextKeyedAPIFactory<ImageWriterControllerLacros>*
  GetFactoryInstance();
  static ImageWriterControllerLacros* Get(content::BrowserContext* context);

 private:
  class ImageWriterClientLacros;
  friend class BrowserContextKeyedAPIFactory<ImageWriterControllerLacros>;

  // BrowserContextKeyedAPI Implementation.
  static const char* service_name() { return "ImageWriterControllerLacros"; }
  // extensions::ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // extensions::ProcessManagerObserver:
  void OnBackgroundHostClose(const std::string& extension_id) override;
  void OnProcessManagerShutdown(extensions::ProcessManager* manager) override;
  void OnExtensionProcessTerminated(
      const extensions::Extension* extension) override;

  void DeletePendingClient(const std::string& extension_id);

  // |browser_context_| is safe to use in the class, since this class is a
  // BrowserContextKeyedAPI with |browser_context_| which will handle the
  // destruction of BrowserContext gracefully by shutting down the service
  // and removing itself from the factory.
  raw_ptr<content::BrowserContext> browser_context_;

  // Pending image writer clients by extension id.
  // For each extension, we only allow one pending remote client to request
  // write operation at a time, which is consistent with OperationManager's
  // logic.
  std::map<std::string, std::unique_ptr<ImageWriterClientLacros>>
      pending_clients_;

  // Listen to extension unloaded notification.
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};

  // Listen to ProcessManagerObserver for ExtensionHost.
  base::ScopedObservation<extensions::ProcessManager,
                          extensions::ProcessManagerObserver>
      process_manager_observation_{this};
};

}  // namespace image_writer

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_CONTROLLER_LACROS_H_
