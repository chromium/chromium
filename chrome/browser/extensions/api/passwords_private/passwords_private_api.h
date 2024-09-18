// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_

#include <optional>
#include <string>

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class PasswordsPrivateRecordPasswordsPageAccessInSettingsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "passwordsPrivate.recordPasswordsPageAccessInSettings",
      PASSWORDSPRIVATE_RECORDPASSWORDSPAGEACCESSINSETTINGS)

 protected:
  ~PasswordsPrivateRecordPasswordsPageAccessInSettingsFunction() override =
      default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateChangeCredentialFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.changeCredential",
                             PASSWORDSPRIVATE_CHANGECREDENTIAL)

 protected:
  ~PasswordsPrivateChangeCredentialFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateRemoveCredentialFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.removeCredential",
                             PASSWORDSPRIVATE_REMOVECREDENTIAL)

 protected:
  ~PasswordsPrivateRemoveCredentialFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateRemovePasswordExceptionFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.removePasswordException",
                             PASSWORDSPRIVATE_REMOVEPASSWORDEXCEPTION)

 protected:
  ~PasswordsPrivateRemovePasswordExceptionFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateUndoRemoveSavedPasswordOrExceptionFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "passwordsPrivate.undoRemoveSavedPasswordOrException",
      PASSWORDSPRIVATE_UNDOREMOVESAVEDPASSWORDOREXCEPTION)

 protected:
  ~PasswordsPrivateUndoRemoveSavedPasswordOrExceptionFunction() override =
      default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateRequestPlaintextPasswordFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.requestPlaintextPassword",
                             PASSWORDSPRIVATE_REQUESTPLAINTEXTPASSWORD)

 protected:
  ~PasswordsPrivateRequestPlaintextPasswordFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void GotPassword(std::optional<std::u16string> password);
};

class PasswordsPrivateRequestCredentialsDetailsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.requestCredentialsDetails",
                             PASSWORDSPRIVATE_REQUESTCREDENTIALSDETAILS)
 protected:
  ~PasswordsPrivateRequestCredentialsDetailsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void GotPasswords(const PasswordsPrivateDelegate::UiEntries& entries);
};

class PasswordsPrivateGetSavedPasswordListFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.getSavedPasswordList",
                             PASSWORDSPRIVATE_GETSAVEDPASSWORDLIST)

 protected:
  ~PasswordsPrivateGetSavedPasswordListFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void GotList(const PasswordsPrivateDelegate::UiEntries& entries);
};

class PasswordsPrivateGetCredentialGroupsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.getCredentialGroups",
                             PASSWORDSPRIVATE_GETCREDENTIALGROUPS)

 protected:
  ~PasswordsPrivateGetCredentialGroupsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateGetPasswordExceptionListFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.getPasswordExceptionList",
                             PASSWORDSPRIVATE_GETPASSWORDEXCEPTIONLIST)

 protected:
  ~PasswordsPrivateGetPasswordExceptionListFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void GotList(const PasswordsPrivateDelegate::ExceptionEntries& entries);
};

class PasswordsPrivateMovePasswordsToAccountFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.movePasswordsToAccount",
                             PASSWORDSPRIVATE_MOVEPASSWORDSTOACCOUNT)

 protected:
  ~PasswordsPrivateMovePasswordsToAccountFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateFetchFamilyMembersFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.fetchFamilyMembers",
                             PASSWORDSPRIVATE_FETCHFAMILYMEMBERS)

 protected:
  ~PasswordsPrivateFetchFamilyMembersFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void FamilyFetchCompleted(
      const api::passwords_private::FamilyFetchResults& results);
};

class PasswordsPrivateSharePasswordFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.sharePassword",
                             PASSWORDSPRIVATE_SHAREPASSWORD)

 protected:
  ~PasswordsPrivateSharePasswordFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateImportPasswordsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.importPasswords",
                             PASSWORDSPRIVATE_IMPORTPASSWORDS)

 protected:
  ~PasswordsPrivateImportPasswordsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void ImportRequestCompleted(
      const api::passwords_private::ImportResults& results);
};

class PasswordsPrivateContinueImportFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.continueImport",
                             PASSWORDSPRIVATE_CONTINUEIMPORT)

 protected:
  ~PasswordsPrivateContinueImportFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void ImportCompleted(const api::passwords_private::ImportResults& results);
};

class PasswordsPrivateResetImporterFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.resetImporter",
                             PASSWORDSPRIVATE_RESETIMPORTER)

 protected:
  ~PasswordsPrivateResetImporterFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateExportPasswordsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.exportPasswords",
                             PASSWORDSPRIVATE_EXPORTPASSWORDS)

 protected:
  ~PasswordsPrivateExportPasswordsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void ExportRequestCompleted(const std::string& error);
};

class PasswordsPrivateRequestExportProgressStatusFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.requestExportProgressStatus",
                             PASSWORDSPRIVATE_REQUESTEXPORTPROGRESSSTATUS)

 protected:
  ~PasswordsPrivateRequestExportProgressStatusFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateIsAccountStorageEnabledFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.isAccountStorageEnabled",
                             PASSWORDSPRIVATE_ISACCOUNTSTORAGEENABLED)

 protected:
  ~PasswordsPrivateIsAccountStorageEnabledFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateSetAccountStorageEnabledFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.setAccountStorageEnabled",
                             PASSWORDSPRIVATE_SETACCOUNTSTORAGEENABLED)

 protected:
  ~PasswordsPrivateSetAccountStorageEnabledFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateGetInsecureCredentialsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.getInsecureCredentials",
                             PASSWORDSPRIVATE_GETINSECURECREDENTIALS)

 protected:
  ~PasswordsPrivateGetInsecureCredentialsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateGetCredentialsWithReusedPasswordFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "passwordsPrivate.getCredentialsWithReusedPassword",
      PASSWORDSPRIVATE_GETCREDENTIALSWITHREUSEDPASSWORD)

 protected:
  ~PasswordsPrivateGetCredentialsWithReusedPasswordFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateMuteInsecureCredentialFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.muteInsecureCredential",
                             PASSWORDSPRIVATE_MUTEINSECURECREDENTIAL)

 protected:
  ~PasswordsPrivateMuteInsecureCredentialFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateUnmuteInsecureCredentialFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.unmuteInsecureCredential",
                             PASSWORDSPRIVATE_UNMUTEINSECURECREDENTIAL)

 protected:
  ~PasswordsPrivateUnmuteInsecureCredentialFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateStartPasswordCheckFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.startPasswordCheck",
                             PASSWORDSPRIVATE_STARTPASSWORDCHECK)

 protected:
  ~PasswordsPrivateStartPasswordCheckFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnStarted(password_manager::BulkLeakCheckService::State state);
};

class PasswordsPrivateGetPasswordCheckStatusFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.getPasswordCheckStatus",
                             PASSWORDSPRIVATE_GETPASSWORDCHECKSTATUS)

 protected:
  ~PasswordsPrivateGetPasswordCheckStatusFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateIsAccountStoreDefaultFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.isAccountStoreDefault",
                             PASSWORDSPRIVATE_ISACCOUNTSTOREDEFAULT)

 protected:
  ~PasswordsPrivateIsAccountStoreDefaultFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateGetUrlCollectionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.getUrlCollection",
                             PASSWORDSPRIVATE_GETURLCOLLECTION)

 protected:
  ~PasswordsPrivateGetUrlCollectionFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateAddPasswordFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.addPassword",
                             PASSWORDSPRIVATE_ADDPASSWORD)

 protected:
  ~PasswordsPrivateAddPasswordFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateExtendAuthValidityFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.extendAuthValidity",
                             PASSWORDSPRIVATE_EXTENDAUTHVALIDITY)

 protected:
  ~PasswordsPrivateExtendAuthValidityFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateSwitchBiometricAuthBeforeFillingStateFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "passwordsPrivate.switchBiometricAuthBeforeFillingState",
      PASSWORDSPRIVATE_SWITCHBIOMETRICAUTHBEFOREFILLINGSTATE)

 protected:
  ~PasswordsPrivateSwitchBiometricAuthBeforeFillingStateFunction() override =
      default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnAuthenticationComplete(bool result);
};

class PasswordsPrivateShowAddShortcutDialogFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.showAddShortcutDialog",
                             PASSWORDSPRIVATE_SHOWADDSHORTCUTDIALOG)

 protected:
  ~PasswordsPrivateShowAddShortcutDialogFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateShowExportedFileInShellFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.showExportedFileInShell",
                             PASSWORDSPRIVATE_SHOWEXPORTEDFILEINSHELL)

 protected:
  ~PasswordsPrivateShowExportedFileInShellFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateChangePasswordManagerPinFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.changePasswordManagerPin",
                             PASSWORDSPRIVATE_CHANGEPASSWORDMANAGERPIN)

 protected:
  ~PasswordsPrivateChangePasswordManagerPinFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnPinChangeCompleted(bool success);
};

class PasswordsPrivateIsPasswordManagerPinAvailableFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.isPasswordManagerPinAvailable",
                             PASSWORDSPRIVATE_ISPASSWORDMANAGERPINAVAILABLE)

 protected:
  ~PasswordsPrivateIsPasswordManagerPinAvailableFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnPasswordManagerPinAvailabilityReceived(bool is_available);
};

class PasswordsPrivateDisconnectCloudAuthenticatorFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.disconnectCloudAuthenticator",
                             PASSWORDSPRIVATE_DISCONNECTCLOUDAUTHENTICATOR)

 protected:
  ~PasswordsPrivateDisconnectCloudAuthenticatorFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnDisconnectCloudAuthenticatorCompleted(bool success);
};

class PasswordsPrivateIsConnectedToCloudAuthenticatorFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.isConnectedToCloudAuthenticator",
                             PASSWORDSPRIVATE_ISCONNECTEDTOCLOUDAUTHENTICATOR)

 protected:
  ~PasswordsPrivateIsConnectedToCloudAuthenticatorFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateDeleteAllPasswordManagerDataFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.deleteAllPasswordManagerData",
                             PASSWORDSPRIVATE_DELETEALLPASSWORDMANAGERDATA)

 protected:
  ~PasswordsPrivateDeleteAllPasswordManagerDataFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnDeletionCompleted(bool success);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_
