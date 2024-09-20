// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_API_H_

#include "components/prefs/pref_service.h"
#include "components/user_annotations/user_annotations_types.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {
class AutofillPrivateGetAccountInfoFunction : public ExtensionFunction {
 public:
  AutofillPrivateGetAccountInfoFunction() = default;
  AutofillPrivateGetAccountInfoFunction(
      const AutofillPrivateGetAccountInfoFunction&) = delete;
  AutofillPrivateGetAccountInfoFunction& operator=(
      const AutofillPrivateGetAccountInfoFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getAccountInfo",
                             AUTOFILLPRIVATE_GETACCOUNTINFO)

 protected:
  ~AutofillPrivateGetAccountInfoFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateSaveAddressFunction : public ExtensionFunction {
 public:
  AutofillPrivateSaveAddressFunction() = default;
  AutofillPrivateSaveAddressFunction(
      const AutofillPrivateSaveAddressFunction&) = delete;
  AutofillPrivateSaveAddressFunction& operator=(
      const AutofillPrivateSaveAddressFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.saveAddress",
                             AUTOFILLPRIVATE_SAVEADDRESS)

 protected:
  ~AutofillPrivateSaveAddressFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetCountryListFunction : public ExtensionFunction {
 public:
  AutofillPrivateGetCountryListFunction() = default;
  AutofillPrivateGetCountryListFunction(
      const AutofillPrivateGetCountryListFunction&) = delete;
  AutofillPrivateGetCountryListFunction& operator=(
      const AutofillPrivateGetCountryListFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getCountryList",
                             AUTOFILLPRIVATE_GETCOUNTRYLIST)

 protected:
  ~AutofillPrivateGetCountryListFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetAddressComponentsFunction : public ExtensionFunction {
 public:
  AutofillPrivateGetAddressComponentsFunction() = default;
  AutofillPrivateGetAddressComponentsFunction(
      const AutofillPrivateGetAddressComponentsFunction&) = delete;
  AutofillPrivateGetAddressComponentsFunction& operator=(
      const AutofillPrivateGetAddressComponentsFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getAddressComponents",
                             AUTOFILLPRIVATE_GETADDRESSCOMPONENTS)

 protected:
  ~AutofillPrivateGetAddressComponentsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetAddressListFunction : public ExtensionFunction {
 public:
  AutofillPrivateGetAddressListFunction() = default;
  AutofillPrivateGetAddressListFunction(
      const AutofillPrivateGetAddressListFunction&) = delete;
  AutofillPrivateGetAddressListFunction& operator=(
      const AutofillPrivateGetAddressListFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getAddressList",
                             AUTOFILLPRIVATE_GETADDRESSLIST)

 protected:
  ~AutofillPrivateGetAddressListFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateSaveCreditCardFunction : public ExtensionFunction {
 public:
  AutofillPrivateSaveCreditCardFunction() = default;
  AutofillPrivateSaveCreditCardFunction(
      const AutofillPrivateSaveCreditCardFunction&) = delete;
  AutofillPrivateSaveCreditCardFunction& operator=(
      const AutofillPrivateSaveCreditCardFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.saveCreditCard",
                             AUTOFILLPRIVATE_SAVECREDITCARD)

 protected:
  ~AutofillPrivateSaveCreditCardFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateRemoveEntryFunction : public ExtensionFunction {
 public:
  AutofillPrivateRemoveEntryFunction() = default;
  AutofillPrivateRemoveEntryFunction(
      const AutofillPrivateRemoveEntryFunction&) = delete;
  AutofillPrivateRemoveEntryFunction& operator=(
      const AutofillPrivateRemoveEntryFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.removeEntry",
                             AUTOFILLPRIVATE_REMOVEENTRY)

 protected:
  ~AutofillPrivateRemoveEntryFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetCreditCardListFunction : public ExtensionFunction {
 public:
  AutofillPrivateGetCreditCardListFunction() = default;
  AutofillPrivateGetCreditCardListFunction(
      const AutofillPrivateGetCreditCardListFunction&) = delete;
  AutofillPrivateGetCreditCardListFunction& operator=(
      const AutofillPrivateGetCreditCardListFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getCreditCardList",
                             AUTOFILLPRIVATE_GETCREDITCARDLIST)

 protected:
  ~AutofillPrivateGetCreditCardListFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateMigrateCreditCardsFunction : public ExtensionFunction {
 public:
  AutofillPrivateMigrateCreditCardsFunction() = default;
  AutofillPrivateMigrateCreditCardsFunction(
      const AutofillPrivateMigrateCreditCardsFunction&) = delete;
  AutofillPrivateMigrateCreditCardsFunction& operator=(
      const AutofillPrivateMigrateCreditCardsFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.migrateCreditCards",
                             AUTOFILLPRIVATE_MIGRATECREDITCARDS)

 protected:
  ~AutofillPrivateMigrateCreditCardsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateLogServerCardLinkClickedFunction
    : public ExtensionFunction {
 public:
  AutofillPrivateLogServerCardLinkClickedFunction() = default;
  AutofillPrivateLogServerCardLinkClickedFunction(
      const AutofillPrivateLogServerCardLinkClickedFunction&) = delete;
  AutofillPrivateLogServerCardLinkClickedFunction& operator=(
      const AutofillPrivateLogServerCardLinkClickedFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.logServerCardLinkClicked",
                             AUTOFILLPRIVATE_SERVERCARDLINKCLICKED)

 protected:
  ~AutofillPrivateLogServerCardLinkClickedFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateLogServerIbanLinkClickedFunction
    : public ExtensionFunction {
 public:
  AutofillPrivateLogServerIbanLinkClickedFunction() = default;
  AutofillPrivateLogServerIbanLinkClickedFunction(
      const AutofillPrivateLogServerIbanLinkClickedFunction&) = delete;
  AutofillPrivateLogServerIbanLinkClickedFunction& operator=(
      const AutofillPrivateLogServerIbanLinkClickedFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.logServerIbanLinkClicked",
                             AUTOFILLPRIVATE_SERVERIBANLINKCLICKED)

 protected:
  ~AutofillPrivateLogServerIbanLinkClickedFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateSaveIbanFunction : public ExtensionFunction {
 public:
  AutofillPrivateSaveIbanFunction() = default;
  AutofillPrivateSaveIbanFunction(const AutofillPrivateSaveIbanFunction&) =
      delete;
  AutofillPrivateSaveIbanFunction& operator=(
      const AutofillPrivateSaveIbanFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.saveIban",
                             AUTOFILLPRIVATE_SAVEIBAN)

 protected:
  ~AutofillPrivateSaveIbanFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetIbanListFunction : public ExtensionFunction {
 public:
  AutofillPrivateGetIbanListFunction() = default;
  AutofillPrivateGetIbanListFunction(
      const AutofillPrivateGetIbanListFunction&) = delete;
  AutofillPrivateGetIbanListFunction& operator=(
      const AutofillPrivateGetIbanListFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getIbanList",
                             AUTOFILLPRIVATE_GETIBANLIST)

 protected:
  ~AutofillPrivateGetIbanListFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateIsValidIbanFunction : public ExtensionFunction {
 public:
  AutofillPrivateIsValidIbanFunction() = default;
  AutofillPrivateIsValidIbanFunction(
      const AutofillPrivateIsValidIbanFunction&) = delete;
  AutofillPrivateIsValidIbanFunction& operator=(
      const AutofillPrivateIsValidIbanFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.isValidIban",
                             AUTOFILLPRIVATE_ISVALIDIBAN)

 protected:
  ~AutofillPrivateIsValidIbanFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateAddVirtualCardFunction : public ExtensionFunction {
 public:
  AutofillPrivateAddVirtualCardFunction() = default;
  AutofillPrivateAddVirtualCardFunction(
      const AutofillPrivateAddVirtualCardFunction&) = delete;
  AutofillPrivateAddVirtualCardFunction& operator=(
      const AutofillPrivateAddVirtualCardFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.addVirtualCard",
                             AUTOFILLPRIVATE_ADDVIRTUALCARD)

 protected:
  ~AutofillPrivateAddVirtualCardFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateRemoveVirtualCardFunction : public ExtensionFunction {
 public:
  AutofillPrivateRemoveVirtualCardFunction() = default;
  AutofillPrivateRemoveVirtualCardFunction(
      const AutofillPrivateRemoveVirtualCardFunction&) = delete;
  AutofillPrivateRemoveVirtualCardFunction& operator=(
      const AutofillPrivateRemoveVirtualCardFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.removeVirtualCard",
                             AUTOFILLPRIVATE_REMOVEVIRTUALCARD)

 protected:
  ~AutofillPrivateRemoveVirtualCardFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction
    : public ExtensionFunction {
 public:
  AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction() = default;
  AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction(
      const AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction&) =
      delete;
  AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction& operator=(
      const AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction&) =
      delete;
  DECLARE_EXTENSION_FUNCTION(
      "autofillPrivate.authenticateUserAndFlipMandatoryAuthToggle",
      AUTOFILLPRIVATE_AUTHENTICATEUSERANDFLIPMANDATORYAUTHTOGGLE)

 protected:
  ~AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction()
      override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void UpdateMandatoryAuthTogglePref(bool reauth_succeeded);
};

// Performs a local reauth before releasing data if reauth is enabled.
class AutofillPrivateGetLocalCardFunction : public ExtensionFunction {
 public:
  AutofillPrivateGetLocalCardFunction() = default;
  AutofillPrivateGetLocalCardFunction(
      const AutofillPrivateGetLocalCardFunction&) = delete;
  AutofillPrivateGetLocalCardFunction& operator=(
      const AutofillPrivateGetLocalCardFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getLocalCard",
                             AUTOFILLPRIVATE_GETLOCALCARD)

 protected:
  ~AutofillPrivateGetLocalCardFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnReauthFinished(bool can_retrieve);
  void ReturnCreditCard();
};

class AutofillPrivateCheckIfDeviceAuthAvailableFunction
    : public ExtensionFunction {
 public:
  AutofillPrivateCheckIfDeviceAuthAvailableFunction() = default;
  AutofillPrivateCheckIfDeviceAuthAvailableFunction(
      const AutofillPrivateCheckIfDeviceAuthAvailableFunction&) = delete;
  AutofillPrivateCheckIfDeviceAuthAvailableFunction& operator=(
      const AutofillPrivateCheckIfDeviceAuthAvailableFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.checkIfDeviceAuthAvailable",
                             AUTOFILLPRIVATE_CHECKIFDEVICEAUTHAVAILABLE)

 protected:
  ~AutofillPrivateCheckIfDeviceAuthAvailableFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateBulkDeleteAllCvcsFunction : public ExtensionFunction {
 public:
  AutofillPrivateBulkDeleteAllCvcsFunction() = default;
  AutofillPrivateBulkDeleteAllCvcsFunction(
      const AutofillPrivateBulkDeleteAllCvcsFunction&) = delete;
  AutofillPrivateBulkDeleteAllCvcsFunction& operator=(
      const AutofillPrivateBulkDeleteAllCvcsFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.bulkDeleteAllCvcs",
                             AUTOFILLPRIVATE_BULKDELETEALLCVCS)

 protected:
  ~AutofillPrivateBulkDeleteAllCvcsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateSetAutofillSyncToggleEnabledFunction
    : public ExtensionFunction {
 public:
  AutofillPrivateSetAutofillSyncToggleEnabledFunction() = default;
  AutofillPrivateSetAutofillSyncToggleEnabledFunction(
      const AutofillPrivateSetAutofillSyncToggleEnabledFunction&) = delete;
  AutofillPrivateSetAutofillSyncToggleEnabledFunction& operator=(
      const AutofillPrivateSetAutofillSyncToggleEnabledFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.setAutofillSyncToggleEnabled",
                             AUTOFILLPRIVATE_SETAUTOFILLSYNCTOGGLEENABLED)

 protected:
  ~AutofillPrivateSetAutofillSyncToggleEnabledFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetUserAnnotationsEntriesFunction
    : public ExtensionFunction {
 public:
  AutofillPrivateGetUserAnnotationsEntriesFunction() = default;
  AutofillPrivateGetUserAnnotationsEntriesFunction(
      const AutofillPrivateGetUserAnnotationsEntriesFunction&) = delete;
  AutofillPrivateGetUserAnnotationsEntriesFunction& operator=(
      const AutofillPrivateGetUserAnnotationsEntriesFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getUserAnnotationsEntries",
                             AUTOFILLPRIVATE_GETUSERANNOTATIONSENTRIES)

 protected:
  ~AutofillPrivateGetUserAnnotationsEntriesFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnEntriesRetrieved(user_annotations::UserAnnotationsEntries results);
};

class AutofillPrivateDeleteUserAnnotationsEntryFunction
    : public ExtensionFunction {
 public:
  AutofillPrivateDeleteUserAnnotationsEntryFunction() = default;
  AutofillPrivateDeleteUserAnnotationsEntryFunction(
      const AutofillPrivateDeleteUserAnnotationsEntryFunction&) = delete;
  AutofillPrivateDeleteUserAnnotationsEntryFunction& operator=(
      const AutofillPrivateDeleteUserAnnotationsEntryFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.deleteUserAnnotationsEntry",
                             AUTOFILLPRIVATE_DELETEUSERANNOTATIONSENTRY)

 protected:
  ~AutofillPrivateDeleteUserAnnotationsEntryFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnEntryDeleted();
};

class AutofillPrivateDeleteAllUserAnnotationsEntriesFunction
    : public ExtensionFunction {
 public:
  AutofillPrivateDeleteAllUserAnnotationsEntriesFunction() = default;
  AutofillPrivateDeleteAllUserAnnotationsEntriesFunction(
      const AutofillPrivateDeleteAllUserAnnotationsEntriesFunction&) = delete;
  AutofillPrivateDeleteAllUserAnnotationsEntriesFunction& operator=(
      const AutofillPrivateDeleteAllUserAnnotationsEntriesFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.deleteAllUserAnnotationsEntries",
                             AUTOFILLPRIVATE_DELETEALLUSERANNOTATIONSENTRIES)

 protected:
  ~AutofillPrivateDeleteAllUserAnnotationsEntriesFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnAllEntriesDeleted();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_API_H_
