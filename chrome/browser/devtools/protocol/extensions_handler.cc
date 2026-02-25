// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/extensions_handler.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_browser_context_manager.h"
#include "chrome/browser/devtools/protocol/extensions.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/storage/storage_area_namespace.h"
#include "extensions/browser/api/storage/storage_utils.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/unpacked_installer.h"
#include "extensions/common/manifest.h"

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
    if (!host->GetProcessHost()) {
      return false;
    }

    return extensions::storage_utils::CanRendererAccessExtensionStorage(
        *context, extension, /*storage_area=*/std::nullopt,
        /*render_frame_host=*/nullptr, *host->GetProcessHost());
  }

  // Allow a page or frame target to access extension storage if it is
  // associated with a renderer that hosts an extension origin or has an
  // extension injected into it.
  if (host->GetType() == ChromeDevToolsManagerDelegate::kTypeBackgroundPage ||
      host->GetType() == content::DevToolsAgentHost::kTypePage ||
      host->GetType() == content::DevToolsAgentHost::kTypeFrame) {
    if (!host->GetWebContents() ||
        !host->GetWebContents()->GetPrimaryMainFrame()) {
      return false;
    }

    bool can_access_storage = false;

    // The content/ layer doesn't expose a way for us to get the frame
    // associated with DevToolsAgentHost. As a compromise, allow access if any
    // frame associated with the WebContents has access. This is safe as there
    // are no instances where a client can attach to one frame in a WebContents
    // but not another.
    host->GetWebContents()
        ->GetPrimaryMainFrame()
        ->ForEachRenderFrameHostWithAction(
            [&extension,
             &can_access_storage](content::RenderFrameHost* render_frame_host) {
              if (extensions::storage_utils::CanRendererAccessExtensionStorage(
                      *render_frame_host->GetBrowserContext(), extension,
                      /*storage_area=*/std::nullopt, render_frame_host,
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

protocol::DispatchResponse ExtensionsHandler::TriggerAction(
    const std::string& extension_id,
    const std::string& target_id) {
  if (!allow_loading_extensions_) {
    return protocol::Response::ServerError("Method not allowed");
  }

  auto host = content::DevToolsAgentHost::GetForId(target_id);
  if (host == nullptr) {
    return protocol::Response::ServerError(
        "cannot retrieve host for the provided id");
  }

  if (host->GetType() != content::DevToolsAgentHost::kTypeTab) {
    return protocol::Response::ServerError(
        "Action can only be triggered on a tab target.");
  }

  content::WebContents* web_contents = host->GetWebContents();
  if (web_contents == nullptr) {
    return protocol::Response::ServerError("Tab target has no WebContents.");
  }

  BrowserWindowInterface* browser =
      extensions::browser_window_util::GetBrowserForTabContents(*web_contents);

  ExtensionsContainer* extension_container =
      ExtensionsContainer::From(*browser);

  ToolbarActionViewModel* extension_model =
      extension_container->GetActionForId(extension_id);

  extension_model->ExecuteUserAction(
      ExtensionActionViewModel::InvocationSource::kCdp);

  return protocol::DispatchResponse::Success();
}

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
    const std::optional<bool> enable_in_incognito,
    std::unique_ptr<ExtensionsHandler::LoadUnpackedCallback> callback) {
  if (!allow_loading_extensions_) {
    std::move(callback)->sendFailure(
        protocol::Response::ServerError("Method not available."));
    return;
  }

  content::BrowserContext* context = ProfileManager::GetLastUsedProfile();
  DCHECK(context);
  scoped_refptr<extensions::UnpackedInstaller> installer(
      extensions::UnpackedInstaller::Create(context));
  installer->set_be_noisy_on_failure(false);
  if (enable_in_incognito.has_value()) {
    installer->set_allow_incognito_access(enable_in_incognito.value());
  }
  installer->set_completion_callback(
      base::BindOnce(&ExtensionsHandler::OnLoaded, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
  installer->Load(base::FilePath(base::FilePath::FromUTF8Unsafe(path)));
}

void ExtensionsHandler::OnLoaded(std::unique_ptr<LoadUnpackedCallback> callback,
                                 const extensions::Extension* extension,
                                 const base::FilePath& path,
                                 const std::u16string& err) {
  if (err.empty()) {
    std::move(callback)->sendSuccess(extension->id());
    return;
  }

  std::move(callback)->sendFailure(
      protocol::Response::InvalidRequest(base::UTF16ToUTF8(err)));
}

protocol::Response ExtensionsHandler::GetExtensions(
    std::unique_ptr<protocol::Array<protocol::Extensions::ExtensionInfo>>*
        out_result) {
  if (!allow_loading_extensions_) {
    return protocol::Response::ServerError("Method not available.");
  }
  content::BrowserContext* context =
      DevToolsBrowserContextManager::GetInstance().GetDefaultBrowserContext();

  if (!context) {
    return protocol::Response::ServerError("Could not find browser context.");
  }

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(context);
  if (!registry) {
    return protocol::Response::ServerError(
        "Could not find extension registry.");
  }

  auto result =
      std::make_unique<protocol::Array<protocol::Extensions::ExtensionInfo>>();
  extensions::ExtensionSet all_extensions =
      registry->GenerateInstalledExtensionsSet();

  for (const auto& extension : all_extensions) {
    if (extensions::Manifest::IsUnpackedLocation(extension->location())) {
      bool is_enabled =
          registry->enabled_extensions().Contains(extension->id());
      result->emplace_back(protocol::Extensions::ExtensionInfo::Create()
                               .SetId(extension->id())
                               .SetName(extension->name())
                               .SetVersion(extension->VersionString())
                               .SetEnabled(is_enabled)
                               .SetPath(extension->path().AsUTF8Unsafe())
                               .Build());
    }
  }

  *out_result = std::move(result);
  return protocol::Response::Success();
}

void ExtensionsHandler::Uninstall(const protocol::String& id,
                                  std::unique_ptr<UninstallCallback> callback) {
  if (!allow_loading_extensions_) {
    std::move(callback)->sendFailure(
        protocol::Response::ServerError("Method not available."));
    return;
  }

  content::BrowserContext* context = ProfileManager::GetLastUsedProfile();
  DCHECK(context);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(context);
  const extensions::Extension* extension = registry->GetInstalledExtension(id);
  if (!extension) {
    std::move(callback)->sendFailure(protocol::Response::ServerError(
        "Uninstall failed. Reason: could not find extension."));
    return;
  }
  if (extension->location() != extensions::mojom::ManifestLocation::kUnpacked) {
    std::move(callback)->sendFailure(protocol::Response::ServerError(
        "Uninstall failed. Reason: extension is not an unpacked extension."));
    return;
  }

  std::u16string error;
  bool initiated =
      extensions::ExtensionRegistrar::Get(context)->UninstallExtension(
          id, extensions::UNINSTALL_REASON_USER_INITIATED, &error,
          base::BindOnce(&ExtensionsHandler::OnUninstalled,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
  if (!initiated) {
    std::move(callback)->sendFailure(protocol::Response::ServerError(
        "Uninstall failed. Reason: " + base::UTF16ToUTF8(error)));
  }
}

void ExtensionsHandler::OnUninstalled(
    std::unique_ptr<UninstallCallback> callback) {
  std::move(callback)->sendSuccess();
}

void ExtensionsHandler::GetStorageItems(
    const protocol::String& id,
    const protocol::String& storage_area,
    std::unique_ptr<protocol::Array<protocol::String>> keys,
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
      keys ? std::optional(std::move(*keys)) : std::nullopt,
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

  base::DictValue data = std::move(*result.data);
  std::move(callback)->sendSuccess(
      std::make_unique<base::DictValue>(std::move(data)));
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
