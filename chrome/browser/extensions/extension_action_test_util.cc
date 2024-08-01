// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_test_util.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace extensions {
namespace extension_action_test_util {

namespace {

size_t GetPageActionCount(content::WebContents* web_contents,
                          bool only_count_visible) {
  DCHECK(web_contents);
  size_t count = 0u;
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  ToolbarActionsModel* toolbar_model = ToolbarActionsModel::Get(profile);
  const base::flat_set<ToolbarActionsModel::ActionId>& toolbar_action_ids =
      toolbar_model->action_ids();
  ExtensionActionManager* action_manager =
      ExtensionActionManager::Get(web_contents->GetBrowserContext());
  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(profile)->enabled_extensions();
  for (const ToolbarActionsModel::ActionId& action_id : toolbar_action_ids) {
    const Extension* extension = enabled_extensions.GetByID(action_id);
    ExtensionAction* extension_action =
        action_manager->GetExtensionAction(*extension);
    if (extension_action &&
        extension_action->action_type() == ActionInfo::Type::kPage &&
        (!only_count_visible || extension_action->GetIsVisible(tab_id.id()))) {
      ++count;
    }
  }
  return count;
}

// Creates a new ToolbarActionsModel for the given |context|.
std::unique_ptr<KeyedService> BuildToolbarModel(
    content::BrowserContext* context) {
  return std::make_unique<ToolbarActionsModel>(
      Profile::FromBrowserContext(context),
      extensions::ExtensionPrefs::Get(context));
}

// Creates a new ToolbarActionsModel for the given profile, optionally
// triggering the extension system's ready signal.
ToolbarActionsModel* CreateToolbarModelImpl(Profile* profile,
                                            bool wait_for_ready) {
  ToolbarActionsModel* model = ToolbarActionsModel::Get(profile);
  if (model) {
    return model;
  }

  // No existing model means it's a new profile (since we, by default, don't
  // create the ToolbarModel in testing).
  ToolbarActionsModelFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&BuildToolbarModel));
  model = ToolbarActionsModel::Get(profile);
  if (wait_for_ready) {
    // Fake the extension system ready signal.
    // HACK ALERT! In production, the ready task on ExtensionSystem (and most
    // everything else on it, too) is shared between incognito and normal
    // profiles, but a TestExtensionSystem doesn't have the concept of "shared".
    // Because of this, we have to set any new profile's TestExtensionSystem's
    // ready task, too.
    static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile))->
        SetReady();
    // Run tasks posted to TestExtensionSystem.
    base::RunLoop().RunUntilIdle();
  }

  return model;
}

}  // namespace

size_t GetVisiblePageActionCount(content::WebContents* web_contents) {
  return GetPageActionCount(web_contents, true);
}

size_t GetTotalPageActionCount(content::WebContents* web_contents) {
  return GetPageActionCount(web_contents, false);
}

ToolbarActionsModel* CreateToolbarModelForProfile(Profile* profile) {
  return CreateToolbarModelImpl(profile, true);
}

ToolbarActionsModel* CreateToolbarModelForProfileWithoutWaitingForReady(
    Profile* profile) {
  return CreateToolbarModelImpl(profile, false);
}

}  // namespace extension_action_test_util
}  // namespace extensions
