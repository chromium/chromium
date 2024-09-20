// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_api.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

namespace {

using ResponseAction = ExtensionFunction::ResponseAction;

constexpr char kNoDelegateError[] =
    "Operation failed because PasswordsPrivateDelegate wasn't created.";

constexpr char kPasswordManagerDisabledByPolicy[] =
    "Operation failed because CredentialsEnableService policy is set to false "
    "by admin.";

scoped_refptr<PasswordsPrivateDelegate> GetDelegate(
    content::BrowserContext* browser_context) {
  return PasswordsPrivateDelegateFactory::GetForBrowserContext(
      browser_context,
      /*create=*/false);
}

bool IsPasswordManagerDisabledByPolicy(
    content::BrowserContext* browser_context) {
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_context);
  return !prefs->GetBoolean(
             password_manager::prefs::kCredentialsEnableService) &&
         prefs->IsManagedPreference(
             password_manager::prefs::kCredentialsEnableService);
}

}  // namespace

// PasswordsPrivateRecordPasswordsPageAccessInSettingsFunction
ResponseAction
PasswordsPrivateRecordPasswordsPageAccessInSettingsFunction::Run() {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kChromeSettings);
  return RespondNow(NoArguments());
}

// PasswordsPrivateChangeCredentialFunction
ResponseAction PasswordsPrivateChangeCredentialFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::ChangeCredential::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  bool success =
      GetDelegate(browser_context())->ChangeCredential(parameters->credential);
  if (success) {
    return RespondNow(NoArguments());
  }
  return RespondNow(Error(
      "Could not change the credential. Either the arguments are not valid or "
      "the credential does not exist"));
}

// PasswordsPrivateRemoveCredentialFunction
ResponseAction PasswordsPrivateRemoveCredentialFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::RemoveCredential::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  GetDelegate(browser_context())
      ->RemoveCredential(parameters->id, parameters->from_stores);
  return RespondNow(NoArguments());
}

// PasswordsPrivateRemovePasswordExceptionFunction
ResponseAction PasswordsPrivateRemovePasswordExceptionFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::RemovePasswordException::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  GetDelegate(browser_context())->RemovePasswordException(parameters->id);
  return RespondNow(NoArguments());
}

// PasswordsPrivateUndoRemoveSavedPasswordOrExceptionFunction
ResponseAction
PasswordsPrivateUndoRemoveSavedPasswordOrExceptionFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  GetDelegate(browser_context())->UndoRemoveSavedPasswordOrException();
  return RespondNow(NoArguments());
}

// PasswordsPrivateRequestPlaintextPasswordFunction
ResponseAction PasswordsPrivateRequestPlaintextPasswordFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::RequestPlaintextPassword::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  GetDelegate(browser_context())
      ->RequestPlaintextPassword(
          parameters->id, parameters->reason,
          base::BindOnce(
              &PasswordsPrivateRequestPlaintextPasswordFunction::GotPassword,
              this),
          GetSenderWebContents());

  // GotPassword() might respond before we reach this point.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void PasswordsPrivateRequestPlaintextPasswordFunction::GotPassword(
    std::optional<std::u16string> password) {
  if (password) {
    Respond(WithArguments(std::move(*password)));
    return;
  }

  Respond(Error(base::StringPrintf(
      "Could not obtain plaintext password. Either the user is not "
      "authenticated or no password with id = %d could be found.",
      api::passwords_private::RequestPlaintextPassword::Params::Create(args())
          ->id)));
}

// PasswordsPrivateRequestCredentialDetailsFunction
ResponseAction PasswordsPrivateRequestCredentialsDetailsFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::RequestCredentialsDetails::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  GetDelegate(browser_context())
      ->RequestCredentialsDetails(
          parameters->ids,
          base::BindOnce(
              &PasswordsPrivateRequestCredentialsDetailsFunction::GotPasswords,
              this),
          GetSenderWebContents());

  // GotPasswords() might have responded before we reach this point.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void PasswordsPrivateRequestCredentialsDetailsFunction::GotPasswords(
    const PasswordsPrivateDelegate::UiEntries& entries) {
  if (!entries.empty()) {
    Respond(ArgumentList(
        api::passwords_private::RequestCredentialsDetails::Results::Create(
            entries)));
    return;
  }

  Respond(Error(
      "Could not obtain password entry. Either the user is not "
      "authenticated or no credential with matching ids could be found."));
}

// PasswordsPrivateGetSavedPasswordListFunction
ResponseAction PasswordsPrivateGetSavedPasswordListFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  GetDelegate(browser_context())
      ->GetSavedPasswordsList(base::BindOnce(
          &PasswordsPrivateGetSavedPasswordListFunction::GotList, this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void PasswordsPrivateGetSavedPasswordListFunction::GotList(
    const PasswordsPrivateDelegate::UiEntries& list) {
  Respond(ArgumentList(
      api::passwords_private::GetSavedPasswordList::Results::Create(list)));
}

// PasswordsPrivateGetCredentialGroupsFunction
ResponseAction PasswordsPrivateGetCredentialGroupsFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  return RespondNow(
      ArgumentList(api::passwords_private::GetCredentialGroups::Results::Create(
          GetDelegate(browser_context())->GetCredentialGroups())));
}

// PasswordsPrivateGetPasswordExceptionListFunction
ResponseAction PasswordsPrivateGetPasswordExceptionListFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  GetDelegate(browser_context())
      ->GetPasswordExceptionsList(base::BindOnce(
          &PasswordsPrivateGetPasswordExceptionListFunction::GotList, this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void PasswordsPrivateGetPasswordExceptionListFunction::GotList(
    const PasswordsPrivateDelegate::ExceptionEntries& entries) {
  Respond(ArgumentList(
      api::passwords_private::GetPasswordExceptionList::Results::Create(
          entries)));
}

// PasswordsPrivateMovePasswordToAccountFunction
ResponseAction PasswordsPrivateMovePasswordsToAccountFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::MovePasswordsToAccount::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  GetDelegate(browser_context())
      ->MovePasswordsToAccount(parameters->ids, GetSenderWebContents());
  return RespondNow(NoArguments());
}

// PasswordsPrivateFetchFamilyMembersFunction
ResponseAction PasswordsPrivateFetchFamilyMembersFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  GetDelegate(browser_context())
      ->FetchFamilyMembers(base::BindOnce(
          &PasswordsPrivateFetchFamilyMembersFunction::FamilyFetchCompleted,
          this));

  // `FamilyFetchCompleted()` might respond before we reach this point.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

// PasswordsPrivateSharePasswordFunction
ResponseAction PasswordsPrivateSharePasswordFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  // TODO(crbug.com/40268194): Respond with an error if arguments are not valid
  // (password doesn't exist, auth validity expired, recipient doesn't have
  // public key or user_id).

  auto parameters =
      api::passwords_private::SharePassword::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  GetDelegate(browser_context())
      ->SharePassword(parameters->id, parameters->recipients);
  return RespondNow(NoArguments());
}

void PasswordsPrivateFetchFamilyMembersFunction::FamilyFetchCompleted(
    const api::passwords_private::FamilyFetchResults& result) {
  Respond(ArgumentList(
      api::passwords_private::FetchFamilyMembers::Results::Create(result)));
}

// PasswordsPrivateImportPasswordsFunction
ResponseAction PasswordsPrivateImportPasswordsFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  if (IsPasswordManagerDisabledByPolicy(browser_context())) {
    return RespondNow(Error(kPasswordManagerDisabledByPolicy));
  }

  auto parameters =
      api::passwords_private::ImportPasswords::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  GetDelegate(browser_context())
      ->ImportPasswords(
          parameters->to_store,
          base::BindOnce(
              &PasswordsPrivateImportPasswordsFunction::ImportRequestCompleted,
              this),
          GetSenderWebContents());

  // `ImportRequestCompleted()` might respond before we reach this point.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void PasswordsPrivateImportPasswordsFunction::ImportRequestCompleted(
    const api::passwords_private::ImportResults& result) {
  Respond(ArgumentList(
      api::passwords_private::ImportPasswords::Results::Create(result)));
}

// PasswordsPrivateContinueImportFunction
ResponseAction PasswordsPrivateContinueImportFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::ContinueImport::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  GetDelegate(browser_context())
      ->ContinueImport(
          parameters->selected_ids,
          base::BindOnce(
              &PasswordsPrivateContinueImportFunction::ImportCompleted, this),
          GetSenderWebContents());

  // `ImportCompleted()` might respond before we reach this point.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void PasswordsPrivateContinueImportFunction::ImportCompleted(
    const api::passwords_private::ImportResults& result) {
  Respond(ArgumentList(
      api::passwords_private::ImportPasswords::Results::Create(result)));
}

// PasswordsPrivateResetImporterFunction
ResponseAction PasswordsPrivateResetImporterFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::ResetImporter::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  GetDelegate(browser_context())->ResetImporter(parameters->delete_file);
  return RespondNow(NoArguments());
}

// PasswordsPrivateExportPasswordsFunction
ResponseAction PasswordsPrivateExportPasswordsFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  GetDelegate(browser_context())
      ->ExportPasswords(
          base::BindOnce(
              &PasswordsPrivateExportPasswordsFunction::ExportRequestCompleted,
              this),
          GetSenderWebContents());
  return RespondLater();
}

void PasswordsPrivateExportPasswordsFunction::ExportRequestCompleted(
    const std::string& error) {
  if (error.empty()) {
    Respond(NoArguments());
  } else {
    Respond(Error(error));
  }
}

// PasswordsPrivateRequestExportProgressStatusFunction
ResponseAction PasswordsPrivateRequestExportProgressStatusFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  return RespondNow(ArgumentList(
      api::passwords_private::RequestExportProgressStatus::Results::Create(
          GetDelegate(browser_context())->GetExportProgressStatus())));
}

// PasswordsPrivateIsAccountStorageEnabledFunction
ResponseAction PasswordsPrivateIsAccountStorageEnabledFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  return RespondNow(
      WithArguments(GetDelegate(browser_context())->IsAccountStorageEnabled()));
}

// PasswordsPrivateSetAccountStorageEnabledFunction
ResponseAction PasswordsPrivateSetAccountStorageEnabledFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::SetAccountStorageEnabled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  GetDelegate(browser_context())
      ->SetAccountStorageEnabled(parameters->enabled, GetSenderWebContents());
  return RespondNow(NoArguments());
}

// PasswordsPrivateGetInsecureCredentialsFunction:
PasswordsPrivateGetInsecureCredentialsFunction::
    ~PasswordsPrivateGetInsecureCredentialsFunction() = default;

ResponseAction PasswordsPrivateGetInsecureCredentialsFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  return RespondNow(ArgumentList(
      api::passwords_private::GetInsecureCredentials::Results::Create(
          GetDelegate(browser_context())->GetInsecureCredentials())));
}

// PasswordsPrivateGetCredentialsWithReusedPasswordFunction:
PasswordsPrivateGetCredentialsWithReusedPasswordFunction::
    ~PasswordsPrivateGetCredentialsWithReusedPasswordFunction() = default;

ResponseAction PasswordsPrivateGetCredentialsWithReusedPasswordFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  return RespondNow(ArgumentList(
      api::passwords_private::GetCredentialsWithReusedPassword::Results::Create(
          GetDelegate(browser_context())->GetCredentialsWithReusedPassword())));
}

// PasswordsPrivateMuteInsecureCredentialFunction:
PasswordsPrivateMuteInsecureCredentialFunction::
    ~PasswordsPrivateMuteInsecureCredentialFunction() = default;

ResponseAction PasswordsPrivateMuteInsecureCredentialFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::MuteInsecureCredential::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  if (!GetDelegate(browser_context())
           ->MuteInsecureCredential(parameters->credential)) {
    return RespondNow(
        Error("Could not mute the insecure credential. Probably no matching "
              "password could be found."));
  }

  return RespondNow(NoArguments());
}

// PasswordsPrivateUnmuteInsecureCredentialFunction:
PasswordsPrivateUnmuteInsecureCredentialFunction::
    ~PasswordsPrivateUnmuteInsecureCredentialFunction() = default;

ResponseAction PasswordsPrivateUnmuteInsecureCredentialFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::UnmuteInsecureCredential::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  if (!GetDelegate(browser_context())
           ->UnmuteInsecureCredential(parameters->credential)) {
    return RespondNow(
        Error("Could not unmute the insecure credential. Probably no matching "
              "password could be found."));
  }

  return RespondNow(NoArguments());
}

// PasswordsPrivateStartPasswordCheckFunction:
PasswordsPrivateStartPasswordCheckFunction::
    ~PasswordsPrivateStartPasswordCheckFunction() = default;

ResponseAction PasswordsPrivateStartPasswordCheckFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  GetDelegate(browser_context())
      ->StartPasswordCheck(base::BindOnce(
          &PasswordsPrivateStartPasswordCheckFunction::OnStarted, this));

  // OnStarted() might respond before we reach this point.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void PasswordsPrivateStartPasswordCheckFunction::OnStarted(
    password_manager::BulkLeakCheckService::State state) {
  const bool is_running =
      state == password_manager::BulkLeakCheckService::State::kRunning;
  Respond(is_running ? NoArguments()
                     : Error("Starting password check failed."));
}

// PasswordsPrivateGetPasswordCheckStatusFunction:
PasswordsPrivateGetPasswordCheckStatusFunction::
    ~PasswordsPrivateGetPasswordCheckStatusFunction() = default;

ResponseAction PasswordsPrivateGetPasswordCheckStatusFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  return RespondNow(ArgumentList(
      api::passwords_private::GetPasswordCheckStatus::Results::Create(
          GetDelegate(browser_context())->GetPasswordCheckStatus())));
}

// PasswordsPrivateIsAccountStoreDefaultFunction
ResponseAction PasswordsPrivateIsAccountStoreDefaultFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  return RespondNow(
      WithArguments(GetDelegate(browser_context())
                        ->IsAccountStoreDefault(GetSenderWebContents())));
}

// PasswordsPrivateGetUrlCollectionFunction:
ResponseAction PasswordsPrivateGetUrlCollectionFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::GetUrlCollection::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  const std::optional<api::passwords_private::UrlCollection> url_collection =
      GetDelegate(browser_context())->GetUrlCollection(parameters->url);
  if (!url_collection) {
    return RespondNow(
        Error("Provided string doesn't meet password URL requirements. Either "
              "the format is invalid or the scheme is not unsupported."));
  }

  return RespondNow(
      ArgumentList(api::passwords_private::GetUrlCollection::Results::Create(
          url_collection.value())));
}

// PasswordsPrivateAddPasswordFunction
ResponseAction PasswordsPrivateAddPasswordFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  if (IsPasswordManagerDisabledByPolicy(browser_context())) {
    return RespondNow(Error(kPasswordManagerDisabledByPolicy));
  }

  auto parameters = api::passwords_private::AddPassword::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  if (!GetDelegate(browser_context())
           ->AddPassword(parameters->options.url,
                         base::UTF8ToUTF16(parameters->options.username),
                         base::UTF8ToUTF16(parameters->options.password),
                         base::UTF8ToUTF16(parameters->options.note),
                         parameters->options.use_account_store,
                         GetSenderWebContents())) {
    return RespondNow(Error(
        "Could not add the password. Either the url is invalid, the password "
        "is empty or an entry with such origin and username already exists."));
  }

  return RespondNow(NoArguments());
}

// PasswordsPrivateExtendAuthValidityFunction
ResponseAction PasswordsPrivateExtendAuthValidityFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  GetDelegate(browser_context())->RestartAuthTimer();
  return RespondNow(NoArguments());
}

// PasswordsPrivateSwitchBiometricAuthBeforeFillingStateFunction
ResponseAction
PasswordsPrivateSwitchBiometricAuthBeforeFillingStateFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  GetDelegate(browser_context())
      ->SwitchBiometricAuthBeforeFillingState(
          GetSenderWebContents(),
          base::BindOnce(
              &PasswordsPrivateSwitchBiometricAuthBeforeFillingStateFunction::
                  OnAuthenticationComplete,
              this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void PasswordsPrivateSwitchBiometricAuthBeforeFillingStateFunction::
    OnAuthenticationComplete(bool result) {
  Respond(WithArguments(result));
}

// PasswordsPrivateShowExportedFileInShellFunction
ResponseAction PasswordsPrivateShowExportedFileInShellFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  auto parameters =
      api::passwords_private::ShowExportedFileInShell::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  GetDelegate(browser_context())
      ->ShowExportedFileInShell(GetSenderWebContents(), parameters->file_path);
  return RespondNow(NoArguments());
}

// PasswordsPrivateShowAddShortcutDialogFunction
ResponseAction PasswordsPrivateShowAddShortcutDialogFunction::Run() {
  if (!GetDelegate(browser_context())) {
    return RespondNow(Error(kNoDelegateError));
  }

  GetDelegate(browser_context())->ShowAddShortcutDialog(GetSenderWebContents());
  return RespondNow(NoArguments());
}

// PasswordsPrivateChangePasswordManagerPinFunction
ResponseAction PasswordsPrivateChangePasswordManagerPinFunction::Run() {
  if (auto delegate = GetDelegate(browser_context())) {
    delegate->ChangePasswordManagerPin(
        GetSenderWebContents(),
        base::BindOnce(&PasswordsPrivateChangePasswordManagerPinFunction::
                           OnPinChangeCompleted,
                       this));
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  return RespondNow(Error(kNoDelegateError));
}

void PasswordsPrivateChangePasswordManagerPinFunction::OnPinChangeCompleted(
    bool success) {
  Respond(WithArguments(success));
}

// PasswordsPrivateIsPasswordManagerPinAvailableFunction
ResponseAction PasswordsPrivateIsPasswordManagerPinAvailableFunction::Run() {
  if (auto delegate = GetDelegate(browser_context())) {
    delegate->IsPasswordManagerPinAvailable(
        GetSenderWebContents(),
        base::BindOnce(&PasswordsPrivateIsPasswordManagerPinAvailableFunction::
                           OnPasswordManagerPinAvailabilityReceived,
                       this));
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  return RespondNow(Error(kNoDelegateError));
}

void PasswordsPrivateIsPasswordManagerPinAvailableFunction::
    OnPasswordManagerPinAvailabilityReceived(bool is_available) {
  Respond(WithArguments(is_available));
}

// PasswordsPrivateDisconnectCloudAuthenticatorFunction
ResponseAction PasswordsPrivateDisconnectCloudAuthenticatorFunction::Run() {
  if (auto delegate = GetDelegate(browser_context())) {
    delegate->DisconnectCloudAuthenticator(
        GetSenderWebContents(),
        base::BindOnce(&PasswordsPrivateDisconnectCloudAuthenticatorFunction::
                           OnDisconnectCloudAuthenticatorCompleted,
                       this));
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  return RespondNow(Error(kNoDelegateError));
}

void PasswordsPrivateDisconnectCloudAuthenticatorFunction::
    OnDisconnectCloudAuthenticatorCompleted(bool success) {
  Respond(WithArguments(success));
}

// PasswordsPrivateIsConnectedToCloudAuthenticatorFunction
ResponseAction PasswordsPrivateIsConnectedToCloudAuthenticatorFunction::Run() {
  if (auto delegate = GetDelegate(browser_context())) {
    return RespondNow(WithArguments(
        delegate->IsConnectedToCloudAuthenticator(GetSenderWebContents())));
  }

  return RespondNow(Error(kNoDelegateError));
}

// PasswordsPrivateDeleteAllPasswordManagerDataFunction
ResponseAction PasswordsPrivateDeleteAllPasswordManagerDataFunction::Run() {
  if (auto delegate = GetDelegate(browser_context())) {
    delegate->DeleteAllPasswordManagerData(
        GetSenderWebContents(),
        base::BindOnce(&PasswordsPrivateDeleteAllPasswordManagerDataFunction::
                           OnDeletionCompleted,
                       this));
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  return RespondNow(Error(kNoDelegateError));
}

void PasswordsPrivateDeleteAllPasswordManagerDataFunction::OnDeletionCompleted(
    bool success) {
  Respond(WithArguments(success));
}

}  // namespace extensions
