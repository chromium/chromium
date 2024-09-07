// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/quick_unlock_private/quick_unlock_private_api.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/quick_unlock_private/quick_unlock_private_ash_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"

namespace extensions {

namespace quick_unlock_private = api::quick_unlock_private;
namespace SetModes = quick_unlock_private::SetModes;
namespace GetActiveModes = quick_unlock_private::GetActiveModes;
namespace CheckCredential = quick_unlock_private::CheckCredential;
namespace GetCredentialRequirements =
    quick_unlock_private::GetCredentialRequirements;
namespace GetAvailableModes = quick_unlock_private::GetAvailableModes;
namespace OnActiveModesChanged = quick_unlock_private::OnActiveModesChanged;

using CredentialProblem = quick_unlock_private::CredentialProblem;
using CredentialCheck = quick_unlock_private::CredentialCheck;
using CredentialRequirements = quick_unlock_private::CredentialRequirements;
using QuickUnlockMode = quick_unlock_private::QuickUnlockMode;

using AuthToken = ash::quick_unlock::AuthToken;
using QuickUnlockModeList = std::vector<QuickUnlockMode>;

using ActiveModeCallback = base::OnceCallback<void(const QuickUnlockModeList&)>;

namespace {

const char kModesAndCredentialsLengthMismatch[] =
    "|modes| and |credentials| must have the same number of elements";
const char kMultipleModesNotSupported[] =
    "At most one quick unlock mode can be active.";
const char kPinDisabledByPolicy[] = "PIN unlock has been disabled by policy";

const char kInvalidPIN[] = "Invalid PIN.";
const char kInvalidCredential[] = "Invalid credential.";
const char kInternalError[] = "Internal error.";
const char kWeakCredential[] = "Weak credential.";

const char kAuthTokenExpired[] = "Authentication token invalid or expired.";

// PINs greater in length than |kMinLengthForWeakPin| will be checked for
// weakness.
constexpr size_t kMinLengthForNonWeakPin = 2U;

// A list of the most commmonly used PINs, whose digits are not all the same,
// increasing or decreasing. This list is taken from
// www.datagenetics.com/blog/september32012/.
constexpr const char* kMostCommonPins[] = {"1212", "1004", "2000", "6969",
                                           "1122", "1313", "2001", "1010"};

// Returns the active set of quick unlock modes.
void ComputeActiveModes(Profile* profile, ActiveModeCallback result) {
  user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  ash::quick_unlock::PinBackend::GetInstance()->IsSet(
      user->GetAccountId(),
      base::BindOnce(
          [](ActiveModeCallback result, bool is_set) {
            QuickUnlockModeList modes;
            if (is_set)
              modes.push_back(quick_unlock_private::QuickUnlockMode::kPin);
            std::move(result).Run(modes);
          },
          std::move(result)));
}

// Returns true if |a| and |b| contain the same elements. The elements do not
// need to be in the same order.
bool AreModesEqual(const QuickUnlockModeList& a, const QuickUnlockModeList& b) {
  if (a.size() != b.size())
    return false;

  // This is a slow comparison algorithm, but the number of entries in |a| and
  // |b| will always be very low (0-3 items) so it doesn't matter.
  for (auto mode : a) {
    if (!base::Contains(b, mode))
      return false;
  }

  return true;
}

bool IsPinNumeric(const std::string& pin) {
  return base::ranges::all_of(pin, ::isdigit);
}

// Reads and sanitizes the pin length policy.
// Returns the minimum and maximum required pin lengths.
// - minimum must be at least 1.
// - maximum must be at least |min_length|, or 0.
std::pair<int, int> GetSanitizedPolicyPinMinMaxLength(
    PrefService* pref_service) {
  int min_length = std::max(
      pref_service->GetInteger(ash::prefs::kPinUnlockMinimumLength), 1);
  int max_length =
      pref_service->GetInteger(ash::prefs::kPinUnlockMaximumLength);
  max_length = max_length > 0 ? std::max(max_length, min_length) : 0;

  DCHECK_GE(min_length, 1);
  DCHECK_GE(max_length, 0);
  return std::make_pair(min_length, max_length);
}

// Checks whether a given |pin| has any problems given the PIN min/max policies
// in |pref_service|. Returns CREDENTIAL_PROBLEM_NONE if |pin| has no problems,
// or another CREDENTIAL_PROBLEM_ enum value to indicate the detected problem.
CredentialProblem GetCredentialProblemForPin(const std::string& pin,
                                             PrefService* pref_service) {
  int min_length;
  int max_length;
  std::tie(min_length, max_length) =
      GetSanitizedPolicyPinMinMaxLength(pref_service);

  // Check if the PIN is shorter than the minimum specified length.
  if (pin.size() < static_cast<size_t>(min_length))
    return CredentialProblem::kTooShort;

  // If the maximum specified length is zero, there is no maximum length.
  // Otherwise check if the PIN is longer than the maximum specified length.
  if (max_length != 0 && pin.size() > static_cast<size_t>(max_length))
    return CredentialProblem::kTooLong;

  return CredentialProblem::kNone;
}

// Checks if a given |pin| is weak or not. A PIN is considered weak if it:
// a) is on this list - www.datagenetics.com/blog/september32012/
// b) has all the same digits
// c) each digit is one larger than the previous digit
// d) each digit is one smaller than the previous digit
// Note: A 9 followed by a 0 is not considered increasing, and a 0 followed by
// a 9 is not considered decreasing.
bool IsPinDifficultEnough(const std::string& pin) {
  // If the pin length is |kMinLengthForNonWeakPin| or less, there is no need to
  // check for same character and increasing pin.
  if (pin.size() <= kMinLengthForNonWeakPin)
    return true;

  // Check if it is on the list of most common PINs.
  if (base::Contains(kMostCommonPins, pin))
    return false;

  // Check for same digits, increasing and decreasing PIN simultaneously.
  bool is_same = true;
  // TODO(sammiequon): Should longer PINs (5+) be still subjected to this?
  bool is_increasing = true;
  bool is_decreasing = true;
  for (size_t i = 1; i < pin.length(); ++i) {
    const char previous = pin[i - 1];
    const char current = pin[i];

    is_same = is_same && (current == previous);
    is_increasing = is_increasing && (current == previous + 1);
    is_decreasing = is_decreasing && (current == previous - 1);
  }

  // PIN is considered weak if any of these conditions is met.
  if (is_same || is_increasing || is_decreasing)
    return false;

  return true;
}

Profile* GetActiveProfile(content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // When OOBE continues in-session as Furst Run UI, it is still executed
  // under Sign-In profile.
  if (ash::ProfileHelper::IsSigninProfile(profile))
    return ProfileManager::GetPrimaryUserProfile();

  return profile;
}

std::optional<std::string> CheckTokenValidity(
    content::BrowserContext* browser_context,
    const std::string& token) {
  if (!ash::AuthSessionStorage::Get()->IsValid(token)) {
    return kAuthTokenExpired;
  }
  return std::nullopt;
}

}  // namespace

// quickUnlockPrivate.getAuthToken

QuickUnlockPrivateGetAuthTokenFunction::QuickUnlockPrivateGetAuthTokenFunction()
    : chrome_details_(this) {}

QuickUnlockPrivateGetAuthTokenFunction::
    ~QuickUnlockPrivateGetAuthTokenFunction() = default;

ExtensionFunction::ResponseAction
QuickUnlockPrivateGetAuthTokenFunction::Run() {
  std::optional<quick_unlock_private::GetAuthToken::Params> params =
      quick_unlock_private::GetAuthToken::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = GetActiveProfile(browser_context());

  DCHECK(!helper_);
  helper_ = std::make_unique<QuickUnlockPrivateGetAuthTokenHelper>(
      profile, params->account_password);
  auto callback = base::BindOnce(
      &QuickUnlockPrivateGetAuthTokenFunction::OnResult, WrapRefCounted(this));
  helper_->Run(std::move(callback));
  return RespondLater();
}

void QuickUnlockPrivateGetAuthTokenFunction::OnResult(
    std::optional<api::quick_unlock_private::TokenInfo> token_info,
    std::optional<ash::AuthenticationError> error) {
  if (!token_info.has_value()) {
    DCHECK(error.has_value());
    Respond(Error(kInvalidCredential));
    return;
  }

  Respond(ArgumentList(quick_unlock_private::GetAuthToken::Results::Create(
      std::move(*token_info))));
}

// quickUnlockPrivate.setLockScreenEnabled

QuickUnlockPrivateSetLockScreenEnabledFunction::
    QuickUnlockPrivateSetLockScreenEnabledFunction()
    : chrome_details_(this) {}

QuickUnlockPrivateSetLockScreenEnabledFunction::
    ~QuickUnlockPrivateSetLockScreenEnabledFunction() {}

ExtensionFunction::ResponseAction
QuickUnlockPrivateSetLockScreenEnabledFunction::Run() {
  auto params =
      quick_unlock_private::SetLockScreenEnabled::Params::Create(args());
  std::optional<std::string> error =
      CheckTokenValidity(browser_context(), params->token);
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }

  GetActiveProfile(browser_context())
      ->GetPrefs()
      ->SetBoolean(ash::prefs::kEnableAutoScreenLock, params->enabled);

  return RespondNow(ArgumentList(
      quick_unlock_private::SetLockScreenEnabled::Results::Create()));
}

// quickUnlockPrivate.setPinAutosubmitEnabled

QuickUnlockPrivateSetPinAutosubmitEnabledFunction::
    QuickUnlockPrivateSetPinAutosubmitEnabledFunction()
    : chrome_details_(this) {}

QuickUnlockPrivateSetPinAutosubmitEnabledFunction::
    ~QuickUnlockPrivateSetPinAutosubmitEnabledFunction() = default;

ExtensionFunction::ResponseAction
QuickUnlockPrivateSetPinAutosubmitEnabledFunction::Run() {
  auto params =
      quick_unlock_private::SetPinAutosubmitEnabled::Params::Create(args());

  std::optional<std::string> error =
      CheckTokenValidity(browser_context(), params->token);
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }

  Profile* profile = GetActiveProfile(browser_context());
  user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);

  ash::quick_unlock::PinBackend::GetInstance()->SetPinAutoSubmitEnabled(
      user->GetAccountId(), params->pin, params->enabled,
      base::BindOnce(&QuickUnlockPrivateSetPinAutosubmitEnabledFunction::
                         HandleSetPinAutoSubmitResult,
                     this));

  return RespondLater();
}

void QuickUnlockPrivateSetPinAutosubmitEnabledFunction::
    HandleSetPinAutoSubmitResult(bool result) {
  Respond(ArgumentList(
      quick_unlock_private::SetPinAutosubmitEnabled::Results::Create(result)));
}

// quickUnlockPrivate.canAuthenticatePin

QuickUnlockPrivateCanAuthenticatePinFunction::
    QuickUnlockPrivateCanAuthenticatePinFunction()
    : chrome_details_(this) {}

QuickUnlockPrivateCanAuthenticatePinFunction::
    ~QuickUnlockPrivateCanAuthenticatePinFunction() = default;

ExtensionFunction::ResponseAction
QuickUnlockPrivateCanAuthenticatePinFunction::Run() {
  Profile* profile = GetActiveProfile(browser_context());
  user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);

  ash::quick_unlock::PinBackend::GetInstance()->CanAuthenticate(
      user->GetAccountId(), ash::quick_unlock::Purpose::kAny,
      base::BindOnce(&QuickUnlockPrivateCanAuthenticatePinFunction::
                         HandleCanAuthenticateResult,
                     this));
  return RespondLater();
}

void QuickUnlockPrivateCanAuthenticatePinFunction::HandleCanAuthenticateResult(
    bool result,
    cryptohome::PinLockAvailability available_at) {
  // |available_at| is ignored.
  Respond(ArgumentList(
      quick_unlock_private::CanAuthenticatePin::Results::Create(result)));
}

// quickUnlockPrivate.getAvailableModes

QuickUnlockPrivateGetAvailableModesFunction::
    QuickUnlockPrivateGetAvailableModesFunction()
    : chrome_details_(this) {}

QuickUnlockPrivateGetAvailableModesFunction::
    ~QuickUnlockPrivateGetAvailableModesFunction() {}

ExtensionFunction::ResponseAction
QuickUnlockPrivateGetAvailableModesFunction::Run() {
  QuickUnlockModeList modes;
  if (!ash::quick_unlock::IsPinDisabledByPolicy(
          GetActiveProfile(browser_context())->GetPrefs(),
          ash::quick_unlock::Purpose::kAny)) {
    modes.push_back(quick_unlock_private::QuickUnlockMode::kPin);
  }

  return RespondNow(ArgumentList(GetAvailableModes::Results::Create(modes)));
}

// quickUnlockPrivate.getActiveModes

QuickUnlockPrivateGetActiveModesFunction::
    QuickUnlockPrivateGetActiveModesFunction()
    : chrome_details_(this) {}

QuickUnlockPrivateGetActiveModesFunction::
    ~QuickUnlockPrivateGetActiveModesFunction() = default;

ExtensionFunction::ResponseAction
QuickUnlockPrivateGetActiveModesFunction::Run() {
  ComputeActiveModes(
      GetActiveProfile(browser_context()),
      base::BindOnce(
          &QuickUnlockPrivateGetActiveModesFunction::OnGetActiveModes, this));
  return RespondLater();
}

void QuickUnlockPrivateGetActiveModesFunction::OnGetActiveModes(
    const std::vector<api::quick_unlock_private::QuickUnlockMode>& modes) {
  Respond(ArgumentList(GetActiveModes::Results::Create(modes)));
}

// quickUnlockPrivate.checkCredential

QuickUnlockPrivateCheckCredentialFunction::
    QuickUnlockPrivateCheckCredentialFunction() {}

QuickUnlockPrivateCheckCredentialFunction::
    ~QuickUnlockPrivateCheckCredentialFunction() {}

ExtensionFunction::ResponseAction
QuickUnlockPrivateCheckCredentialFunction::Run() {
  std::optional<CheckCredential::Params> params_ =
      CheckCredential::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  auto result = std::make_unique<CredentialCheck>();

  // Only handles pins for now.
  if (params_->mode != QuickUnlockMode::kPin) {
    return RespondNow(ArgumentList(CheckCredential::Results::Create(*result)));
  }

  const std::string& credential = params_->credential;

  Profile* profile = GetActiveProfile(browser_context());
  PrefService* pref_service = profile->GetPrefs();
  bool allow_weak =
      pref_service->GetBoolean(ash::prefs::kPinUnlockWeakPinsAllowed);
  bool is_allow_weak_pin_pref_set =
      pref_service->HasPrefPath(ash::prefs::kPinUnlockWeakPinsAllowed);

  // Check and return the problems.
  std::vector<CredentialProblem>& warnings = result->warnings;
  std::vector<CredentialProblem>& errors = result->errors;
  if (!IsPinNumeric(credential))
    errors.push_back(CredentialProblem::kContainsNondigit);

  CredentialProblem length_problem =
      GetCredentialProblemForPin(credential, pref_service);
  if (length_problem != CredentialProblem::kNone) {
    errors.push_back(length_problem);
  }

  if ((!allow_weak || !is_allow_weak_pin_pref_set) &&
      !IsPinDifficultEnough(credential)) {
    auto& log = allow_weak ? warnings : errors;
    log.push_back(CredentialProblem::kTooWeak);
  }

  return RespondNow(ArgumentList(CheckCredential::Results::Create(*result)));
}

QuickUnlockPrivateGetCredentialRequirementsFunction::
    QuickUnlockPrivateGetCredentialRequirementsFunction() {}

QuickUnlockPrivateGetCredentialRequirementsFunction::
    ~QuickUnlockPrivateGetCredentialRequirementsFunction() {}

ExtensionFunction::ResponseAction
QuickUnlockPrivateGetCredentialRequirementsFunction::Run() {
  std::optional<GetCredentialRequirements::Params> params_ =
      GetCredentialRequirements::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  // GetCredentialRequirements could be called before user sign-in, or before
  // the user profile is finished loading during UI initialization in
  // SetupPinKeyboardElement.connectedCallback,
  // Use the sign-in profile from browser_context() in such case.
  // TODO(b/288150711): Revert to `GetActiveProfile` after fix.
  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile =
      active_user && active_user->is_profile_created()
          ? Profile::FromBrowserContext(
                ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
                    active_user))
          : Profile::FromBrowserContext(browser_context());

  auto result = std::make_unique<CredentialRequirements>();
  std::tie(result->min_length, result->max_length) =
      GetSanitizedPolicyPinMinMaxLength(profile->GetPrefs());

  return RespondNow(
      ArgumentList(GetCredentialRequirements::Results::Create(*result)));
}

// quickUnlockPrivate.setModes

QuickUnlockPrivateSetModesFunction::QuickUnlockPrivateSetModesFunction()
    : chrome_details_(this) {}

QuickUnlockPrivateSetModesFunction::~QuickUnlockPrivateSetModesFunction() =
    default;

void QuickUnlockPrivateSetModesFunction::SetModesChangedEventHandlerForTesting(
    const ModesChangedEventHandler& handler) {
  modes_changed_handler_ = handler;
}

ExtensionFunction::ResponseAction QuickUnlockPrivateSetModesFunction::Run() {
  params_ = SetModes::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  if (params_->modes.size() != params_->credentials.size())
    return RespondNow(Error(kModesAndCredentialsLengthMismatch));

  if (params_->modes.size() > 1)
    return RespondNow(Error(kMultipleModesNotSupported));

  std::optional<std::string> error =
      CheckTokenValidity(browser_context(), params_->token);
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }

  // Verify every credential is valid based on policies.
  PrefService* pref_service = GetActiveProfile(browser_context())->GetPrefs();

  // Do not allow setting a PIN if it is disabled by policy. It is disabled
  // on the UI, but users can still reach here via dev tools.
  for (auto& mode : params_->modes) {
    if (mode == QuickUnlockMode::kPin &&
        ash::quick_unlock::IsPinDisabledByPolicy(
            pref_service, ash::quick_unlock::Purpose::kAny)) {
      return RespondNow(Error(kPinDisabledByPolicy));
    }
  }

  // Verify every credential is valid based on policies.
  bool allow_weak =
      pref_service->GetBoolean(ash::prefs::kPinUnlockWeakPinsAllowed);
  for (size_t i = 0; i < params_->modes.size(); ++i) {
    if (params_->credentials[i].empty())
      continue;

    if (params_->modes[i] != QuickUnlockMode::kPin) {
      continue;
    }

    if (!IsPinNumeric(params_->credentials[i]))
      return RespondNow(Error(kInvalidPIN));

    CredentialProblem problem =
        GetCredentialProblemForPin(params_->credentials[i], pref_service);
    if (problem != CredentialProblem::kNone) {
      return RespondNow(Error(kInvalidCredential));
    }

    if (!allow_weak && !IsPinDifficultEnough(params_->credentials[i]))
      return RespondNow(Error(kWeakCredential));
  }

  ComputeActiveModes(
      GetActiveProfile(browser_context()),
      base::BindOnce(&QuickUnlockPrivateSetModesFunction::OnGetActiveModes,
                     this));

  return RespondLater();
}

void QuickUnlockPrivateSetModesFunction::OnGetActiveModes(
    const std::vector<QuickUnlockMode>& initial_modes) {
  initial_modes_ = initial_modes;

  // This function is setup so it is easy to add another quick unlock mode while
  // following all of the invariants, which are:
  //
  // 1: If an unlock type is not specified, it should be deactivated.
  // 2: If a credential for an unlock type is empty, it should not be touched.
  // 3: Otherwise, the credential should be set to the new value.

  bool update_pin = true;
  std::string pin_credential;

  // Compute needed changes.
  DCHECK_EQ(params_->credentials.size(), params_->modes.size());
  for (size_t i = 0; i < params_->modes.size(); ++i) {
    const QuickUnlockMode mode = params_->modes[i];
    const std::string& credential = params_->credentials[i];

    if (mode == quick_unlock_private::QuickUnlockMode::kPin) {
      update_pin = !credential.empty();
      pin_credential = credential;
    }
  }

  // Apply changes.
  if (update_pin) {
    Profile* profile = GetActiveProfile(browser_context());
    user_manager::User* user =
        ash::ProfileHelper::Get()->GetUserByProfile(profile);
    if (pin_credential.empty()) {
      ash::quick_unlock::PinBackend::GetInstance()->Remove(
          user->GetAccountId(), params_->token,
          base::BindOnce(
              &QuickUnlockPrivateSetModesFunction::PinRemoveCallComplete,
              this));
    } else {
      ash::quick_unlock::PinBackend::GetInstance()->Set(
          user->GetAccountId(), params_->token, pin_credential,
          base::BindOnce(
              &QuickUnlockPrivateSetModesFunction::PinSetCallComplete, this));
    }
  } else {
    // No changes to apply. Call result directly.
    ModeChangeComplete(initial_modes_);
  }
}

void QuickUnlockPrivateSetModesFunction::PinSetCallComplete(bool result) {
  if (!result) {
    Respond(Error(kInternalError));
    return;
  }
  ComputeActiveModes(
      GetActiveProfile(browser_context()),
      base::BindOnce(&QuickUnlockPrivateSetModesFunction::ModeChangeComplete,
                     this));
}

void QuickUnlockPrivateSetModesFunction::PinRemoveCallComplete(bool result) {
  ComputeActiveModes(
      GetActiveProfile(browser_context()),
      base::BindOnce(&QuickUnlockPrivateSetModesFunction::ModeChangeComplete,
                     this));
}

void QuickUnlockPrivateSetModesFunction::ModeChangeComplete(
    const std::vector<QuickUnlockMode>& updated_modes) {
  if (!AreModesEqual(initial_modes_, updated_modes))
    FireEvent(updated_modes);

  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(
          GetActiveProfile(browser_context()));
  const ash::UserContext user_context(*user);

  Respond(ArgumentList(SetModes::Results::Create()));
}

// Triggers a quickUnlockPrivate.onActiveModesChanged change event.
void QuickUnlockPrivateSetModesFunction::FireEvent(
    const QuickUnlockModeList& modes) {
  // Allow unit tests to override how events are raised/handled.
  if (modes_changed_handler_) {
    modes_changed_handler_.Run(modes);
    return;
  }

  auto args = OnActiveModesChanged::Create(modes);
  auto event = std::make_unique<Event>(
      events::QUICK_UNLOCK_PRIVATE_ON_ACTIVE_MODES_CHANGED,
      OnActiveModesChanged::kEventName, std::move(args));
  EventRouter::Get(browser_context())->BroadcastEvent(std::move(event));
}

}  // namespace extensions
