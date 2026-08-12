// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/connector_component_extension.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/component_updater/dictation_connector_component_installer.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/file_util.h"

namespace dictation {

namespace {

std::optional<base::DictValue> LoadManifestOnFileThread(
    const base::FilePath& path) {
  CHECK(extensions::GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  std::string error;
  auto manifest = extensions::file_util::LoadManifest(path, &error);
  if (!manifest) {
    // TODO(b/527240600): Improve error reporting throughout this file.
    VLOG(1) << "Can't load "
            << path.Append(FILE_PATH_LITERAL("manifest.json")).AsUTF8Unsafe()
            << ": " << error;
    return std::nullopt;
  }
  return manifest;
}

}  // namespace

ConnectorComponentExtension::ConnectorComponentExtension(Profile* profile)
    : profile_(profile) {
  CHECK(base::FeatureList::IsEnabled(kDictation));

  // Tests and local development can turn off this param to use a plain
  // extension installed manually. In that case we skip all the logic here and
  // unblock.
  if (!kUseComponentExtension.Get()) {
    install_pending_ = false;
    return;
  }
  get_extension_directory_subscription_ =
      component_updater::DictationConnectorComponentInstallerPolicy::
          GetExtensionDirectory(base::BindOnce(
              &ConnectorComponentExtension::InstallConnectorExtension,
              weak_ptr_factory_.GetWeakPtr()));
}

ConnectorComponentExtension::~ConnectorComponentExtension() = default;

void ConnectorComponentExtension::InstallConnectorExtension(
    const base::FilePath& directory) {
  auto* component_loader = extensions::ComponentLoader::Get(profile_);
  if (!component_loader) {
    return;
  }
  if (component_loader->Exists(
          extension_misc::kDictationConnectorExtensionId)) {
    install_pending_ = false;
    DictationKeyedService::Get(profile_)->DidInstallConnector();
    return;
  }

  extensions::GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadManifestOnFileThread, directory),
      base::BindOnce(&ConnectorComponentExtension::OnManifestLoaded,
                     weak_ptr_factory_.GetWeakPtr(), directory));
}

void ConnectorComponentExtension::OnManifestLoaded(
    const base::FilePath& directory,
    std::optional<base::DictValue> manifest) {
  if (!manifest) {
    VLOG(1) << "Failed to load manifest for Dictation Connector.";
    return;
  }
  auto* component_loader = extensions::ComponentLoader::Get(profile_);
  if (!component_loader) {
    return;
  }

  DCHECK(!component_loader->Exists(
      extension_misc::kDictationConnectorExtensionId));

  std::string actual_id =
      component_loader->Add(std::move(manifest.value()), directory);
  DCHECK_EQ(actual_id, extension_misc::kDictationConnectorExtensionId);
  install_pending_ = false;
  DictationKeyedService::Get(profile_)->DidInstallConnector();
}

}  // namespace dictation
