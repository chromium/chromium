// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/api_guard_delegate.h"

#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/hardware_info_delegate.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/util.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "extensions/common/extension.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/account_id/account_id.h"  // nogncheck
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

namespace switches {

// Skips the check for the device manufacturer.
// Used for development/testing.
const char kTelemetryExtensionSkipManufacturerCheckForTesting[] =
    "telemetry-extension-skip-manufacturer-check-for-testing";

}  // namespace switches

namespace {

using CheckCallback = base::OnceCallback<void(bool)>;

// A helper class to check some conditions asynchronously. All the checks will
// be performed sequentially. If any checker returns false, the rest won't be
// executed and `result_callback` will be called with a corresponding
// `error_message`. After checking all the conditions, `result_callback` is
// called with nullopt.
class AsyncConditionChecker {
 public:
  explicit AsyncConditionChecker(
      base::OnceCallback<void(std::optional<std::string>)> result_callback);
  AsyncConditionChecker(AsyncConditionChecker&) = delete;
  AsyncConditionChecker& operator=(AsyncConditionChecker&) = delete;
  ~AsyncConditionChecker();

  // Appends a checker and an error message to return when check fails. A
  // checker is a `OnceCallback`. It should takes a callback and reply a boolean
  // as result to it.
  void AppendChecker(base::OnceCallback<void(CheckCallback)> checker,
                     const std::string& error_message);
  // Same as above, but also accepts a sync checker for convenience. A sync
  // checker is a `OnceCallback` which returns a boolean as result (instead of
  // passing the result to a callback).
  void AppendChecker(base::OnceCallback<bool(void)> checker,
                     const std::string& error_message);

  // Starts the check.
  void Run();

 private:
  void OnCheckFinished(const std::string& error_message, bool result);

  base::OnceCallback<void(std::optional<std::string>)> result_callback_;
  base::queue<std::pair<base::OnceCallback<void(CheckCallback)>, std::string>>
      callback_queue_;

  base::WeakPtrFactory<AsyncConditionChecker> weak_factory_{this};
};

AsyncConditionChecker::AsyncConditionChecker(
    base::OnceCallback<void(std::optional<std::string>)> result_callback)
    : result_callback_(std::move(result_callback)) {}

AsyncConditionChecker::~AsyncConditionChecker() = default;

void AsyncConditionChecker::AppendChecker(
    base::OnceCallback<void(CheckCallback)> checker,
    const std::string& error_message) {
  callback_queue_.push(std::make_pair(std::move(checker), error_message));
}

void AsyncConditionChecker::AppendChecker(
    base::OnceCallback<bool(void)> checker,
    const std::string& error_message) {
  callback_queue_.push(std::make_pair(
      base::BindOnce(
          [](base::OnceCallback<bool(void)> checker, CheckCallback check) {
            std::move(checker).Then(std::move(check)).Run();
          },
          std::move(checker)),
      error_message));
}

void AsyncConditionChecker::Run() {
  if (callback_queue_.empty()) {
    std::move(result_callback_).Run(std::nullopt);
    return;
  }

  auto [checker, error_message] = std::move(callback_queue_.front());
  callback_queue_.pop();
  std::move(checker).Run(base::BindOnce(&AsyncConditionChecker::OnCheckFinished,
                                        weak_factory_.GetWeakPtr(),
                                        error_message));
}

void AsyncConditionChecker::OnCheckFinished(const std::string& error_message,
                                            bool result) {
  if (!result) {
    std::move(result_callback_).Run(error_message);
    return;
  }
  Run();
}

bool IsExtensionForceInstalled(content::BrowserContext* context,
                               const std::string& extension_id) {
  const auto force_install_list =
      extensions::ExtensionManagementFactory::GetForBrowserContext(context)
          ->GetForceInstallList();
  return force_install_list.Find(extension_id) != nullptr;
}

void OnGetManufacturer(const std::string& extension_id,
                       CheckCallback callback,
                       const std::string& actual_manufacturer) {
  const auto& extension_info = GetChromeOSExtensionInfoById(extension_id);
  const auto& expected_manufacturers = extension_info.manufacturers;
  std::move(callback).Run(expected_manufacturers.contains(actual_manufacturer));
}

void IsExpectedManufacturerForExtensionId(const std::string& extension_id,
                                          CheckCallback callback) {
  auto& hardware_info_delegate = HardwareInfoDelegate::Get();
  hardware_info_delegate.GetManufacturer(
      base::BindOnce(&OnGetManufacturer, extension_id, std::move(callback)));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsExtensionUsedByShimlessRMA(content::BrowserContext* context) {
  return ::ash::features::IsShimlessRMA3pDiagnosticsEnabled() &&
         ::ash::IsShimlessRmaAppBrowserContext(context);
}

bool IsCurrentUserAffiliated() {
  auto* active_user = user_manager::UserManager::Get()->GetActiveUser();
  CHECK(active_user);
  return active_user->IsAffiliated();
}

void IsCurrentUserOwnerOnOwnerFetched(CheckCallback callback) {
  std::move(callback).Run(
      user_manager::UserManager::Get()->IsCurrentUserOwner());
}

void IsCurrentUserOwner(content::BrowserContext* context,
                        CheckCallback callback) {
  auto on_owner_fetched = base::IgnoreArgs<const AccountId&>(
      base::BindOnce(&IsCurrentUserOwnerOnOwnerFetched, std::move(callback)));

  user_manager::UserManager::Get()->GetOwnerAccountIdAsync(
      std::move(on_owner_fetched));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool IsExtensionUsedByShimlessRMA(content::BrowserContext* context) {
  // TODO(b/292227137): Shimless RMA App is not enabled in LaCrOS before
  // migrating Shimless RMA to LaCrOS.
  return false;
}

bool IsCurrentUserAffiliated() {
  return policy::PolicyLoaderLacros::IsMainUserAffiliated();
}

bool IsCurrentUserOwner(content::BrowserContext* context) {
  // In order to determine device ownership in LaCrOS, we need to check
  // whether the current Ash user is the device owner (stored in
  // browser init params) and if the current profile is the same profile
  // as the one logged into Ash.
  return BrowserParamsProxy::Get()->IsCurrentUserDeviceOwner() &&
         Profile::FromBrowserContext(context)->IsMainProfile();
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

class ApiGuardDelegateImpl : public ApiGuardDelegate {
 public:
  ApiGuardDelegateImpl();
  ApiGuardDelegateImpl(const ApiGuardDelegateImpl&) = delete;
  ApiGuardDelegateImpl& operator=(const ApiGuardDelegateImpl&) = delete;
  ~ApiGuardDelegateImpl() override;

  // ApiGuardDelegate:
  // As agreed with the privacy team, telemetry APIs can be accessed if all the
  // following constraints are satisfied:
  // 1. The user is either:
  //    a. managed and the extension was force-installed via policy, or
  //    b. the user is the device owner, or
  //    c. the user is in the Shimless RMA flow.
  // 2. The PWA UI associated with the extension must be opened.
  // 3. The device hardware belongs to the OEM associated with the extension.
  void CanAccessApi(content::BrowserContext* context,
                    const extensions::Extension* extension,
                    CanAccessApiCallback callback) override;

 private:
  std::unique_ptr<AsyncConditionChecker> condition_checker_;
};

ApiGuardDelegateImpl::ApiGuardDelegateImpl() = default;

ApiGuardDelegateImpl::~ApiGuardDelegateImpl() = default;

void ApiGuardDelegateImpl::CanAccessApi(content::BrowserContext* context,
                                        const extensions::Extension* extension,
                                        CanAccessApiCallback callback) {
  condition_checker_ =
      std::make_unique<AsyncConditionChecker>(std::move(callback));

  // base::Unretained is safe to use for a pointer to Extension and Context,
  // because both the extension and the context outlive the delegate by
  // definition (since this is executed as part of an extension API call), and
  // these callbacks are bound to the `condition_checker_`, which is bound to
  // the delegate.

  if (IsExtensionUsedByShimlessRMA(context)) {
    // No user to check for the Shimless RMA flow. Note that in this
    // case there is no active user in UserManager.
  } else if (IsCurrentUserAffiliated()) {
    condition_checker_->AppendChecker(
        base::BindOnce(&IsExtensionForceInstalled, base::Unretained(context),
                       extension->id()),
        "This extension is not installed by the admin");
  } else {
    condition_checker_->AppendChecker(
        base::BindOnce(&IsCurrentUserOwner, base::Unretained(context)),
        "This extension is not run by the device owner");
  }

  condition_checker_->AppendChecker(
      base::BindOnce(&IsTelemetryExtensionAppUiOpenAndSecure,
                     base::Unretained(context), base::Unretained(extension)),
      "Companion app UI is not open or not secure");

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTelemetryExtensionSkipManufacturerCheckForTesting)) {
    condition_checker_->AppendChecker(
        base::BindOnce(&IsExpectedManufacturerForExtensionId, extension->id()),
        "This extension is not allowed to access the API on this device");
  }

  condition_checker_->Run();
}

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
