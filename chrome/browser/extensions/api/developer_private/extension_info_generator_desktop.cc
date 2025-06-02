// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/extension_info_generator_desktop.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_allowlist.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/features.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

namespace extensions {

namespace developer = api::developer_private;

ExtensionInfoGenerator::ExtensionInfoGenerator(
    content::BrowserContext* browser_context)
    : ExtensionInfoGeneratorShared(browser_context) {}

ExtensionInfoGenerator::~ExtensionInfoGenerator() = default;

void ExtensionInfoGenerator::FillExtensionInfo(
    const Extension& extension,
    api::developer_private::ExtensionState state,
    api::developer_private::ExtensionInfo info) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);

  // Pinned to toolbar.
  // TODO(crbug.com/40280426): Currently this information is only shown for
  // enabled extensions as only enabled extensions can have actions. However,
  // this information can be found in prefs, so disabled extensiosn can be
  // included as well.
  ToolbarActionsModel* toolbar_actions_model =
      ToolbarActionsModel::Get(profile);
  if (toolbar_actions_model->HasAction(extension.id())) {
    info.pinned_to_toolbar =
        toolbar_actions_model->IsActionPinned(extension.id());
  }

  // MV2 deprecation.
  ManifestV2ExperimentManager* mv2_experiment_manager =
      ManifestV2ExperimentManager::Get(profile);
  CHECK(mv2_experiment_manager);
  info.is_affected_by_mv2_deprecation =
      mv2_experiment_manager->IsExtensionAffected(extension);
  info.did_acknowledge_mv2_deprecation_notice =
      mv2_experiment_manager->DidUserAcknowledgeNotice(extension.id());
  if (info.web_store_url.length() > 0) {
    info.recommendations_url =
        extension_urls::GetNewWebstoreItemRecommendationsUrl(extension.id())
            .spec();
  }

  // Call the super class implementation to fill the rest of the struct.
  ExtensionInfoGeneratorShared::FillExtensionInfo(extension, state,
                                                  std::move(info));
}

}  // namespace extensions
