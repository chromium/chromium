// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_AI_EMBEDDINGS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_AI_EMBEDDINGS_COMPONENT_INSTALLER_H_

#include <memory>

#include "base/files/file_path.h"
class PrefService;

namespace component_updater {

class ComponentInstallerPolicy;
class ComponentUpdateService;

// Factory function for testing.
std::unique_ptr<ComponentInstallerPolicy>
GetAIEmbeddingsComponentInstallerPolicyForTesting();

// Registers the AI Embeddings component with the component update service.
void RegisterAIEmbeddingsComponent(ComponentUpdateService* cus,
                                   PrefService* local_state);

// Delete the AI Embeddings component.
void DeleteAIEmbeddingsComponent(const base::FilePath& user_data_dir);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_AI_EMBEDDINGS_COMPONENT_INSTALLER_H_
