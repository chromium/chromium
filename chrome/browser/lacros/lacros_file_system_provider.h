// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_FILE_SYSTEM_PROVIDER_H_
#define CHROME_BROWSER_LACROS_LACROS_FILE_SYSTEM_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"

// This class has two responsibilities:
//   (1) It receives file system provider events from Ash. These are forwarded
//   to the corresponding main profile file system provider extension.
//   (2) It detects extension loading/unloading in the main profile and forwards
//   events to ash.
class LacrosFileSystemProvider : public crosapi::mojom::FileSystemProvider,
                                 public extensions::ExtensionRegistryObserver {
 public:
  LacrosFileSystemProvider();
  ~LacrosFileSystemProvider() override;
  LacrosFileSystemProvider(const LacrosFileSystemProvider&) = delete;
  LacrosFileSystemProvider& operator=(const LacrosFileSystemProvider&) = delete;

  // crosapi::mojom::FileSystemProvider
  void DeprecatedDeprecatedForwardOperation(
      const std::string& provider,
      int32_t histogram_value,
      const std::string& event_name,
      std::vector<base::Value> args) override;
  void DeprecatedForwardOperation(const std::string& provider,
                                  int32_t histogram_value,
                                  const std::string& event_name,
                                  std::vector<base::Value> args,
                                  ForwardOperationCallback callback) override;
  void ForwardOperation(const std::string& provider,
                        int32_t histogram_value,
                        const std::string& event_name,
                        base::Value::List args,
                        ForwardOperationCallback callback) override;
  void ForwardRequest(const std::string& provider,
                      const std::optional<std::string>& file_system_id,
                      int64_t request_id,
                      int32_t histogram_value,
                      const std::string& event_name,
                      base::Value::List args,
                      ForwardRequestCallback callback) override;
  void CancelRequest(const std::string& provider,
                     const std::optional<std::string>& file_system_id,
                     int64_t request_id) override;

  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

 private:
  // Mojo endpoint that's responsible for receiving messages from Ash.
  mojo::Receiver<crosapi::mojom::FileSystemProvider> receiver_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_observation_{this};

  base::WeakPtrFactory<LacrosFileSystemProvider> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_LACROS_FILE_SYSTEM_PROVIDER_H_
