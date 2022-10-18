// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/api_guard_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/hardware_info_delegate.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/url_pattern_set.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace content {
class BrowserContext;
}

namespace chromeos {

namespace {

std::string OnGetManufacturer(
    base::flat_set<std::string> expected_manufacturers,
    std::string actual_manufacturer) {
  return expected_manufacturers.contains(actual_manufacturer)
             ? ""
             : "This extension is not allowed to access the API on this "
               "device";
}

class ApiGuardDelegateImpl : public ApiGuardDelegate {
 public:
  ApiGuardDelegateImpl() = default;

  ApiGuardDelegateImpl(const ApiGuardDelegateImpl&) = delete;
  ApiGuardDelegateImpl& operator=(const ApiGuardDelegateImpl&) = delete;

  ~ApiGuardDelegateImpl() override = default;

  // ApiGuardDelegate:
  // As agreed with the privacy team, telemetry APIs can be accessed if all the
  // following constraints are satisfied:
  // 1. The user is either:
  //    a. managed and the extension was force-installed via policy, or
  //    b. the user is the device owner.
  // 2. The PWA UI associated with the extension must be opened.
  // 3. The device hardware belongs to the OEM associated with the extension.
  void CanAccessApi(content::BrowserContext* context,
                    const extensions::Extension* extension,
                    CanAccessApiCallback callback) override {
    if (IsUserAffiliated()) {
      if (!IsExtensionForceInstalled(context, extension->id())) {
        std::move(callback).Run("This extension is not installed by the admin");
        return;
      }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    } else if (!IsCurrentUserOwner()) {
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    } else if (!IsCurrentUserOwner(context)) {
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
      std::move(callback).Run("This extension is not run by the device owner");
      return;
    }

    if (!IsPwaUiOpenAndSecure(context, extension)) {
      std::move(callback).Run("Companion PWA UI is not open or not secure");
      return;
    }

    // TODO(b/200676085): figure out a better way to async check different
    // conditions.
    VerifyManufacturer(extension, std::move(callback));
  }

 private:
  bool IsExtensionForceInstalled(content::BrowserContext* context,
                                 const std::string& extension_id) {
    const auto force_install_list =
        extensions::ExtensionManagementFactory::GetForBrowserContext(context)
            ->GetForceInstallList();
    return force_install_list.Find(extension_id) != nullptr;
  }

  bool IsUserAffiliated() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    return user_manager::UserManager::Get()->GetActiveUser()->IsAffiliated();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    return policy::PolicyLoaderLacros::IsMainUserAffiliated();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool IsCurrentUserOwner() {
    return user_manager::UserManager::Get()->IsCurrentUserOwner();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // In order to determine device ownership in LaCrOS, we need to check
  // whether the current Ash user is the device owner (stored in
  // browser init params) and if the current profile is the same profile
  // as the one logged into Ash.
  bool IsCurrentUserOwner(content::BrowserContext* context) {
    return BrowserParamsProxy::Get()->IsCurrentUserDeviceOwner() &&
           Profile::FromBrowserContext(context)->IsMainProfile();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  bool IsPwaUiOpenAndSecure(content::BrowserContext* context,
                            const extensions::Extension* extension) {
    Profile* profile = Profile::FromBrowserContext(context);

    const auto* externally_connectable_info =
        extensions::ExternallyConnectableInfo::Get(extension);

    for (auto* target_browser : *BrowserList::GetInstance()) {
      // Ignore incognito.
      if (target_browser->profile() != profile) {
        continue;
      }

      TabStripModel* target_tab_strip = target_browser->tab_strip_model();
      for (int i = 0; i < target_tab_strip->count(); ++i) {
        content::WebContents* target_contents =
            target_tab_strip->GetWebContentsAt(i);
        if (externally_connectable_info->matches.MatchesURL(
                target_contents->GetLastCommittedURL())) {
          // Ensure the PWA URL connection is secure (e.g. valid certificate).
          const auto visible_security_state =
              security_state::GetVisibleSecurityState(target_contents);
          return security_state::GetSecurityLevel(
                     *visible_security_state,
                     /*used_policy_installed_certificate=*/false) ==
                 security_state::SecurityLevel::SECURE;
        }
      }
    }

    return false;
  }

  void VerifyManufacturer(const extensions::Extension* extension,
                          CanAccessApiCallback callback) {
    const auto extension_info = GetChromeOSExtensionInfoForId(extension->id());
    const auto expected_manufacturers = extension_info.manufacturers;

    // We can expect VerifyManufacturer() to be called at most once for the
    // lifetime of the ApiGuardDelegateImpl because CanAccessApi() can be called
    // once from BaseTelemetryExtensionApiGuardFunction::Run() which can be
    // called at most once for the lifetime of ExtensionFunction. Therefore, it
    // is safe to instantiate |hardware_info_delegate_| here (vs in the ctor).
    hardware_info_delegate_ = HardwareInfoDelegate::Factory::Create();
    hardware_info_delegate_->GetManufacturer(
        base::BindOnce(&OnGetManufacturer, expected_manufacturers)
            .Then(std::move(callback)));
  }

  std::unique_ptr<HardwareInfoDelegate> hardware_info_delegate_;
};

}  // namespace

// static
ApiGuardDelegate::Factory* ApiGuardDelegate::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<ApiGuardDelegate> ApiGuardDelegate::Factory::Create() {
  if (test_factory_) {
    return test_factory_->CreateInstance();
  }
  return base::WrapUnique<ApiGuardDelegate>(new ApiGuardDelegateImpl());
}

// static
void ApiGuardDelegate::Factory::SetForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

ApiGuardDelegate::Factory::Factory() = default;
ApiGuardDelegate::Factory::~Factory() = default;

ApiGuardDelegate::ApiGuardDelegate() = default;
ApiGuardDelegate::~ApiGuardDelegate() = default;

}  // namespace chromeos
