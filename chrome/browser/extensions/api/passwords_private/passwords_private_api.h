// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_

#include <optional>
#include <string>

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
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

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_
