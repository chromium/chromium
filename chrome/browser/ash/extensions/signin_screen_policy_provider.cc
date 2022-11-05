// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/signin_screen_policy_provider.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {
bool g_bypass_checks_for_testing = false;
}  // namespace

SigninScreenPolicyProvider::SigninScreenPolicyProvider() {}

SigninScreenPolicyProvider::~SigninScreenPolicyProvider() {}

std::string SigninScreenPolicyProvider::GetDebugPolicyProviderName() const {
#if DCHECK_IS_ON()
  return "Guard for sign-in screen";
#else
  base::ImmediateCrash();
#endif
}

bool SigninScreenPolicyProvider::UserMayLoad(
    const extensions::Extension* extension,
    std::u16string* error) const {
  if (g_bypass_checks_for_testing)
    return true;
  const extensions::Feature* feature =
      extensions::FeatureProvider::GetBehaviorFeature(
          extensions::behavior_feature::kSigninScreen);
  CHECK(feature);
  extensions::Feature::Availability availability =
      feature->IsAvailableToExtension(extension);

  if (availability.is_available())
    return true;

  LOG(WARNING) << "Denying load of Extension  : " << extension->id() << " / "
               << extension->name() << " because of " << availability.message();

  // Disallow all other extensions.
  if (error) {
    *error =
        l10n_util::GetStringFUTF16(IDS_EXTENSION_CANT_INSTALL_ON_SIGNIN_SCREEN,
                                   base::UTF8ToUTF16(extension->name()),
                                   base::UTF8ToUTF16(extension->id()));
  }
  return false;
}

std::unique_ptr<base::AutoReset<bool>>
GetScopedSigninScreenPolicyProviderDisablerForTesting() {
  return std::make_unique<base::AutoReset<bool>>(&g_bypass_checks_for_testing,
                                                 true);
}

}  // namespace chromeos
