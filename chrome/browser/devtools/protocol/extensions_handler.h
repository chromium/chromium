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
  void LoadUnpacked(const protocol::String& path,
                    std::unique_ptr<LoadUnpackedCallback> callback) override;
  void OnLoaded(std::unique_ptr<LoadUnpackedCallback> callback,
                const extensions::Extension* extension,
                const base::FilePath&,
                const std::string&);
  void GetStorageItems(
      const protocol::String& id,
      const protocol::String& storage_area,
      protocol::Maybe<protocol::Array<protocol::String>> keys,
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
  bool allow_loading_extensions_;

  std::unique_ptr<protocol::Extensions::Frontend> frontend_;
  base::WeakPtrFactory<ExtensionsHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_EXTENSIONS_HANDLER_H_
