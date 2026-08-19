// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_EXTENSIONS_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_EXTENSIONS_HANDLER_H_

#include "chrome/browser/devtools/protocol/extensions.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/common/extension.h"

// Implements the Extensions domain for Chrome DevTools Protocol.
class ExtensionsHandler : public protocol::Extensions::Backend {
 public:
  explicit ExtensionsHandler(protocol::UberDispatcher* dispatcher,
                             const std::string& target_id,
                             bool allow_loading_extensions);

  ExtensionsHandler(const ExtensionsHandler&) = delete;
  ExtensionsHandler& operator=(const ExtensionsHandler&) = delete;

  ~ExtensionsHandler() override;

 private:
  protocol::DispatchResponse TriggerAction(
      const protocol::String& extendsion_id,
      const protocol::String& target_id) override;
  void LoadUnpacked(const protocol::String& path,
                    std::optional<bool> enable_in_incognito,
                    std::unique_ptr<LoadUnpackedCallback> callback) override;
  void OnLoaded(std::unique_ptr<LoadUnpackedCallback> callback,
                const extensions::Extension* extension,
                const base::FilePath&,
                const std::u16string& err);
  protocol::Response GetExtensions(
      std::unique_ptr<protocol::Array<protocol::Extensions::ExtensionInfo>>*
          out_result) override;
  void Uninstall(const protocol::String& id,
                 std::unique_ptr<UninstallCallback> callback) override;
  void OnUninstalled(std::unique_ptr<UninstallCallback> callback);
  void GetStorageItems(
      const protocol::String& id,
      const protocol::String& storage_area,
      std::unique_ptr<protocol::Array<protocol::String>> keys,
      std::unique_ptr<GetStorageItemsCallback> callback) override;
  void OnGetStorageItemsFinished(
      std::unique_ptr<GetStorageItemsCallback> callback,
      extensions::StorageFrontend::GetResult result);
  void SetStorageItems(
      const protocol::String& id,
      const protocol::String& storage_area,
      std::unique_ptr<protocol::DictionaryValue> values,
      std::unique_ptr<SetStorageItemsCallback> callback) override;
  void OnSetStorageItemsFinished(
      std::unique_ptr<SetStorageItemsCallback> callback,
      extensions::StorageFrontend::ResultStatus result);
  void RemoveStorageItems(
      const protocol::String& id,
      const protocol::String& storage_area,
      std::unique_ptr<protocol::Array<protocol::String>> keys,
      std::unique_ptr<RemoveStorageItemsCallback> callback) override;
  void OnRemoveStorageItemsFinished(
      std::unique_ptr<RemoveStorageItemsCallback> callback,
      extensions::StorageFrontend::ResultStatus result);
  void ClearStorageItems(
      const protocol::String& id,
      const protocol::String& storage_area,
      std::unique_ptr<ClearStorageItemsCallback> callback) override;
  void OnClearStorageItemsFinished(
      std::unique_ptr<ClearStorageItemsCallback> callback,
      extensions::StorageFrontend::ResultStatus result);

  const std::string target_id_;

  // This flag enables an extra security layer for `loadUnpacked`,
  // `uninstall`, and `triggerAction`.
  // This security is vital because these functions perform high-risk
  // operations:
  // -   `loadUnpacked` and `uninstall` interact with extensions
  // stored in the user profile.
  // -   `triggerAction` can grant powerful tab permissions to an extension.
  bool allow_loading_extensions_;

  std::unique_ptr<protocol::Extensions::Frontend> frontend_;
  base::WeakPtrFactory<ExtensionsHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_EXTENSIONS_HANDLER_H_
