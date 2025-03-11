// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_scripts_test_util.h"

#include "base/feature_list.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/extension_features.h"

namespace extensions::user_scripts_test_util {

void SetUserScriptsAPIAllowed(Profile* profile,
                              const ExtensionId& extension_id,
                              bool allowed) {
  if (base::FeatureList::IsEnabled(
          extensions_features::kUserScriptUserExtensionToggle)) {
    UserScriptManager* user_script_manager =
        ExtensionSystem::Get(profile)->user_script_manager();
    DCHECK(user_script_manager);
    user_script_manager->SetUserScriptPrefEnabled(
        /*extension_id=*/extension_id, allowed);
  } else {
    extensions::util::SetDeveloperModeForProfile(profile,
                                                 /*in_developer_mode=*/allowed);
  }

  // Wait for the above IPCs to send.
  RendererStartupHelper* renderer_startup_helper =
      RendererStartupHelperFactory::GetForBrowserContext(profile);
  DCHECK(renderer_startup_helper);
  renderer_startup_helper->FlushAllForTesting();
}

}  // namespace extensions::user_scripts_test_util
