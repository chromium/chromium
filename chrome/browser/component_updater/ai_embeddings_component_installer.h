// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_AI_EMBEDDINGS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_AI_EMBEDDINGS_COMPONENT_INSTALLER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "components/component_updater/component_updater_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/update_client/update_client.h"

class PrefService;

namespace component_updater {

class ComponentInstallerPolicy;

// Factory function for testing.
std::unique_ptr<ComponentInstallerPolicy>
GetAIEmbeddingsComponentInstallerPolicyForTesting();

// Returns the generated CRX ID for the AI Embeddings component.
std::string GetAIEmbeddingsComponentId();

// Registers the AI Embeddings component with the component update service.
// The `callback` receives `true` if registration succeeded and `false` if
// blocked by policies or flags.
void RegisterAIEmbeddingsComponent(
    ComponentUpdateService* cus,
    PrefService* local_state,
    base::OnceCallback<void(bool /*registered*/)> callback = base::DoNothing());

// Returns true if the user setting and the enterprise policy allow the AI
// embeddings model to be installed and used.
bool IsAIEmbeddingsModelAllowed(const PrefService* local_state);

// Unregisters the AI embeddings component, which also deletes its files, or,
// if it isn't registered, deletes any files left on disk, and clears the
// download-eligibility pref.
void PurgeAIEmbeddingsComponent(ComponentUpdateService* cus,
                                PrefService& local_state);

// Manages registration/deletion of the AI Embeddings component based on policy
// and eligibility. When the model is not allowed, the component is
// unregistered (if registered) and its files are deleted. When it is allowed
// and a model may be on disk, this also creates the launcher, which then
// watches the setting and policy for the rest of the session.
void ManageAIEmbeddingsComponentRegistration(ComponentUpdateService* cus,
                                             PrefService* local_state);

// Triggers an on-demand update of the AI Embeddings component.
void UpdateAIEmbeddingsComponentOnDemand(
    component_updater::OnDemandUpdater::Priority priority,
    update_client::Callback callback);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_AI_EMBEDDINGS_COMPONENT_INSTALLER_H_
