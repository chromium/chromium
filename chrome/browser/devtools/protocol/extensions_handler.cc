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
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"

ExtensionsHandler::ExtensionsHandler(protocol::UberDispatcher* dispatcher) {
  protocol::Extensions::Dispatcher::wire(dispatcher, this);
}

ExtensionsHandler::~ExtensionsHandler() = default;

void ExtensionsHandler::LoadUnpacked(
    const protocol::String& path,
    std::unique_ptr<ExtensionsHandler::LoadUnpackedCallback> callback) {
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
