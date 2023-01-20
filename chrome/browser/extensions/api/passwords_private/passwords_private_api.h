// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_

#include <string>

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "extensions/browser/extension_function.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

class PasswordsPrivateChangeSavedPasswordFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.changeSavedPassword",
                             PASSWORDSPRIVATE_CHANGESAVEDPASSWORD)

 protected:
  ~PasswordsPrivateChangeSavedPasswordFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateRemoveSavedPasswordFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.removeSavedPassword",
                             PASSWORDSPRIVATE_REMOVESAVEDPASSWORD)

 protected:
  ~PasswordsPrivateRemoveSavedPasswordFunction() override = default;

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
  void GotPassword(absl::optional<std::u16string> password);
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
  void GetList();
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
  void GetList();
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

class PasswordsPrivateCancelExportPasswordsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.cancelExportPasswords",
                             PASSWORDSPRIVATE_CANCELEXPORTPASSWORDS)

 protected:
  ~PasswordsPrivateCancelExportPasswordsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
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

class PasswordsPrivateIsOptedInForAccountStorageFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.isOptedInForAccountStorage",
                             PASSWORDSPRIVATE_ISOPTEDINFORACCOUNTSTORAGE)

 protected:
  ~PasswordsPrivateIsOptedInForAccountStorageFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateOptInForAccountStorageFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.optInForAccountStorage",
                             PASSWORDSPRIVATE_OPTINFORACCOUNTSTORAGE)

 protected:
  ~PasswordsPrivateOptInForAccountStorageFunction() override = default;

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

class PasswordsPrivateRecordChangePasswordFlowStartedFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.recordChangePasswordFlowStarted",
                             PASSWORDSPRIVATE_RECORDCHANGEPASSWORDFLOWSTARTED)

 protected:
  ~PasswordsPrivateRecordChangePasswordFlowStartedFunction() override;

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

class PasswordsPrivateStopPasswordCheckFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.stopPasswordCheck",
                             PASSWORDSPRIVATE_STOPPASSWORDCHECK)

 protected:
  ~PasswordsPrivateStopPasswordCheckFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
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

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_
