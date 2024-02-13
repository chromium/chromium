// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_OPERATION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_OPERATION_MANAGER_H_

#include <map>
#include <string>
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/image_writer_private/image_writer_private_api.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chrome/common/extensions/api/image_writer_private.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace image_writer_api = extensions::api::image_writer_private;

namespace content {
class BrowserContext;
}

namespace extensions {

namespace image_writer {

class Operation;

// Manages image writer operations for the current profile.  Including clean-up
// and message routing.
class OperationManager : public BrowserContextKeyedAPI,
                         public ExtensionRegistryObserver,
                         public ProcessManagerObserver {
 public:
  explicit OperationManager(content::BrowserContext* context);

  OperationManager(const OperationManager&) = delete;
  OperationManager& operator=(const OperationManager&) = delete;

  ~OperationManager() override;

  void Shutdown() override;

  // Starts a WriteFromUrl operation.
  void StartWriteFromUrl(const ExtensionId& extension_id,
                         GURL url,
                         const std::string& hash,
                         const std::string& device_path,
                         Operation::StartWriteCallback callback);

  // Starts a WriteFromFile operation.
  void StartWriteFromFile(const ExtensionId& extension_id,
                          const base::FilePath& path,
                          const std::string& device_path,
                          Operation::StartWriteCallback callback);

  // Cancels the extensions current operation if any.
  void CancelWrite(const ExtensionId& extension_id,
                   Operation::CancelWriteCallback callback);

  // Starts a write that removes the partition table.
  void DestroyPartitions(const ExtensionId& extension_id,
                         const std::string& device_path,
                         Operation::StartWriteCallback callback);

  // Callback for progress events.
  virtual void OnProgress(const ExtensionId& extension_id,
                          image_writer_api::Stage stage,
                          int progress);
  // Callback for completion events.
  virtual void OnComplete(const ExtensionId& extension_id);

  // Callback for error events.
  virtual void OnError(const ExtensionId& extension_id,
                       image_writer_api::Stage stage,
                       int progress,
                       const std::string& error_message);

  // BrowserContextKeyedAPI
  static BrowserContextKeyedAPIFactory<OperationManager>* GetFactoryInstance();
  static OperationManager* Get(content::BrowserContext* context);

  base::WeakPtr<OperationManager> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  static const char* service_name() {
    return "OperationManager";
  }

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnShutdown(ExtensionRegistry* registry) override;

  // ProcessManagerObserver:
  void OnBackgroundHostClose(const ExtensionId& extension_id) override;
  void OnProcessManagerShutdown(ProcessManager* manager) override;
  void OnExtensionProcessTerminated(const Extension* extension) override;

  Operation* GetOperation(const ExtensionId& extension_id);
  void DeleteOperation(const ExtensionId& extension_id);

  // Accessor to the associated profile download folder
  base::FilePath GetAssociatedDownloadFolder();

  friend class BrowserContextKeyedAPIFactory<OperationManager>;
  typedef std::map<ExtensionId, scoped_refptr<Operation> > OperationMap;

  raw_ptr<content::BrowserContext> browser_context_;
  OperationMap operations_;

  // Listen to extension unloaded notification.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  // Listen to ProcessManagerObserver for ExtensionHost.
  base::ScopedObservation<ProcessManager, ProcessManagerObserver>
      process_manager_observation_{this};

  base::WeakPtrFactory<OperationManager> weak_factory_{this};
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_OPERATION_MANAGER_H_
