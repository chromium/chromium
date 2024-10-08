// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/extensions_handler.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/devtools/protocol/extensions.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/storage/storage_area_namespace.h"
#include "extensions/browser/api/storage/storage_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"

namespace {

// Gets an extension with ID `id`. If no extension is found, returns a
// std::nullopt.
std::optional<const extensions::Extension*> GetExtension(
    const std::string& id,
    scoped_refptr<content::DevToolsAgentHost> host) {
  content::BrowserContext* context = host->GetBrowserContext();
  if (!context) {
    return std::nullopt;
  }

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(context);

  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(id);

  return extension ? std::optional(extension) : std::nullopt;
}

// Returns true if `host` should be able to access data from `storage_area` for
// the given `extension`.
bool CanAccessStorage(scoped_refptr<content::DevToolsAgentHost> host,
                      const extensions::Extension& extension,
                      extensions::StorageAreaNamespace storage_area) {
  content::BrowserContext* context = host->GetBrowserContext();
  if (!context) {
    return false;
  }

  // Allow a service worker to access extension storage if it corresponds to
  // the extension whose storage is being accessed.
  if (host->GetType() == content::DevToolsAgentHost::kTypeServiceWorker) {
    return extensions::storage_utils::CanRendererAccessExtensionStorage(
        *context, extension, storage_area, /*render_frame_host=*/nullptr,
        *host->GetProcessHost());
  }

  // Allow a page or frame target to access extension storage if it is
  // associated with a renderer that hosts an extension origin or has an
  // extension injected into it.
  if (host->GetType() == content::DevToolsAgentHost::kTypePage ||
      host->GetType() == content::DevToolsAgentHost::kTypeFrame) {
    bool can_access_storage = false;

    // The content/ layer doesn't expose a way for us to get the frame
    // associated with DevToolsAgentHost. As a compromise, allow access if any
    // frame associated with the WebContents has access. This is safe as there
    // are no instances where a client can attach to one frame in a WebContents
    // but not another.
    host->GetWebContents()
        ->GetPrimaryMainFrame()
        ->ForEachRenderFrameHostWithAction(
            [&storage_area, &extension,
             &can_access_storage](content::RenderFrameHost* render_frame_host) {
              if (extensions::storage_utils::CanRendererAccessExtensionStorage(
                      *render_frame_host->GetBrowserContext(), extension,
                      storage_area, render_frame_host,
                      *render_frame_host->GetProcess())) {
                can_access_storage = true;
                return content::RenderFrameHost::FrameIterationAction::kStop;
              }

              return content::RenderFrameHost::FrameIterationAction::kContinue;
            });

    return can_access_storage;
  }

  return false;
}

struct GetExtensionAndStorageFrontendResult {
  raw_ptr<const extensions::Extension> extension;
  extensions::StorageAreaNamespace storage_namespace;
  raw_ptr<extensions::StorageFrontend> frontend;

  std::optional<std::string> error;
};

GetExtensionAndStorageFrontendResult GetExtensionAndStorageFrontend(
    const std::string& target_id_,
    const protocol::String& extension_id,
    const protocol::String& storage_area) {
  GetExtensionAndStorageFrontendResult result;

  result.extension = nullptr;
  result.frontend = nullptr;

  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  CHECK(host);

  content::BrowserContext* context = host->GetBrowserContext();
  if (!context) {
    result.error = "No associated browser context.";
    return result;
  }

  const extensions::StorageAreaNamespace namespace_result =
      extensions::StorageAreaFromString(storage_area);

  if (namespace_result == extensions::StorageAreaNamespace::kInvalid) {
    result.error = "Storage area is invalid.";
    return result;
  }

  std::optional<const extensions::Extension*> optional_extension =
      GetExtension(extension_id, host);

  if (!optional_extension ||
      !CanAccessStorage(host, **optional_extension, namespace_result)) {
    result.error = "Extension not found.";
    return result;
  }

  result.extension = *optional_extension;
  result.storage_namespace = namespace_result;
  result.frontend = extensions::StorageFrontend::Get(context);

  return result;
}

}  // namespace

ExtensionsHandler::ExtensionsHandler(protocol::UberDispatcher* dispatcher,
                                     const std::string& target_id,
                                     bool allow_loading_extensions)
    : target_id_(target_id),
      allow_loading_extensions_(allow_loading_extensions) {
  protocol::Extensions::Dispatcher::wire(dispatcher, this);
}

ExtensionsHandler::~ExtensionsHandler() = default;

void ExtensionsHandler::LoadUnpacked(
    const protocol::String& path,
    std::unique_ptr<ExtensionsHandler::LoadUnpackedCallback> callback) {
  if (!allow_loading_extensions_) {
    std::move(callback)->sendFailure(
        protocol::Response::ServerError("Method not available."));
    return;
  }

  content::BrowserContext* context = ProfileManager::GetLastUsedProfile();
  DCHECK(context);
  scoped_refptr<extensions::UnpackedInstaller> installer(
      extensions::UnpackedInstaller::Create(
          extensions::ExtensionSystem::Get(context)->extension_service()));
  installer->set_be_noisy_on_failure(false);
  installer->set_completion_callback(
      base::BindOnce(&ExtensionsHandler::OnLoaded, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
  installer->Load(base::FilePath(base::FilePath::FromUTF8Unsafe(path)));
}

void ExtensionsHandler::OnLoaded(std::unique_ptr<LoadUnpackedCallback> callback,
                                 const extensions::Extension* extension,
                                 const base::FilePath& path,
                                 const std::string& err) {
  if (err.empty()) {
    std::move(callback)->sendSuccess(extension->id());
    return;
  }

  std::move(callback)->sendFailure(protocol::Response::InvalidRequest(err));
}

void ExtensionsHandler::GetStorageItems(
    const protocol::String& id,
    const protocol::String& storage_area,
    protocol::Maybe<protocol::Array<protocol::String>> keys,
    std::unique_ptr<ExtensionsHandler::GetStorageItemsCallback> callback) {
  GetExtensionAndStorageFrontendResult result =
      GetExtensionAndStorageFrontend(target_id_, id, storage_area);

  if (result.error) {
    std::move(callback)->sendFailure(
        protocol::Response::InvalidRequest(*result.error));
    return;
  }

  result.frontend->GetValues(
      result.extension.get(), result.storage_namespace,
      keys ? std::optional(keys.value()) : std::nullopt,
      base::BindOnce(&ExtensionsHandler::OnGetStorageItemsFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionsHandler::OnGetStorageItemsFinished(
    std::unique_ptr<ExtensionsHandler::GetStorageItemsCallback> callback,
    extensions::StorageFrontend::GetResult result) {
  if (!result.status.success) {
    std::move(callback)->sendFailure(
        protocol::Response::ServerError(*result.status.error));
    return;
  }

  base::Value::Dict data = std::move(*result.data);
  std::move(callback)->sendSuccess(
      std::make_unique<base::Value::Dict>(std::move(data)));
}

void ExtensionsHandler::SetStorageItems(
    const protocol::String& id,
    const protocol::String& storage_area,
    std::unique_ptr<protocol::DictionaryValue> values,
    std::unique_ptr<SetStorageItemsCallback> callback) {
  GetExtensionAndStorageFrontendResult result =
      GetExtensionAndStorageFrontend(target_id_, id, storage_area);

  if (result.error) {
    std::move(callback)->sendFailure(
        protocol::Response::InvalidRequest(*result.error));
    return;
  }

  result.frontend->Set(
      result.extension.get(), result.storage_namespace, values->Clone(),
      base::BindOnce(&ExtensionsHandler::OnSetStorageItemsFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionsHandler::OnSetStorageItemsFinished(
    std::unique_ptr<SetStorageItemsCallback> callback,
    extensions::StorageFrontend::ResultStatus status) {
  if (!status.success) {
    std::move(callback)->sendFailure(
        protocol::Response::ServerError(*status.error));
    return;
  }

  std::move(callback)->sendSuccess();
}

void ExtensionsHandler::RemoveStorageItems(
    const protocol::String& id,
    const protocol::String& storage_area,
    std::unique_ptr<protocol::Array<protocol::String>> keys,
    std::unique_ptr<RemoveStorageItemsCallback> callback) {
  GetExtensionAndStorageFrontendResult result =
      GetExtensionAndStorageFrontend(target_id_, id, storage_area);

  if (result.error) {
    std::move(callback)->sendFailure(
        protocol::Response::InvalidRequest(*result.error));
    return;
  }

  result.frontend->Remove(
      result.extension.get(), result.storage_namespace, *keys,
      base::BindOnce(&ExtensionsHandler::OnRemoveStorageItemsFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionsHandler::OnRemoveStorageItemsFinished(
    std::unique_ptr<RemoveStorageItemsCallback> callback,
    extensions::StorageFrontend::ResultStatus status) {
  if (!status.success) {
    std::move(callback)->sendFailure(
        protocol::Response::ServerError(*status.error));
    return;
  }

  std::move(callback)->sendSuccess();
}

void ExtensionsHandler::ClearStorageItems(
    const protocol::String& id,
    const protocol::String& storage_area,
    std::unique_ptr<ClearStorageItemsCallback> callback) {
  GetExtensionAndStorageFrontendResult result =
      GetExtensionAndStorageFrontend(target_id_, id, storage_area);

  if (result.error) {
    std::move(callback)->sendFailure(
        protocol::Response::InvalidRequest(*result.error));
    return;
  }

  result.frontend->Clear(
      result.extension.get(), result.storage_namespace,
      base::BindOnce(&ExtensionsHandler::OnClearStorageItemsFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionsHandler::OnClearStorageItemsFinished(
    std::unique_ptr<ClearStorageItemsCallback> callback,
    extensions::StorageFrontend::ResultStatus status) {
  if (!status.success) {
    std::move(callback)->sendFailure(
        protocol::Response::ServerError(*status.error));
    return;
  }

  std::move(callback)->sendSuccess();
}
