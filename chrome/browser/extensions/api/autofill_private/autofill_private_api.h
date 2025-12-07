// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_API_H_

#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace autofill {
class AddressDataManager;
class ContentAutofillClient;
class PaymentsDataManager;
}  // namespace autofill

namespace extensions {

// A small helper class that exposes getters for Autofill's data managers.
class AutofillPrivateExtensionFunction : public ExtensionFunction {
 public:
  AutofillPrivateExtensionFunction() = default;
  AutofillPrivateExtensionFunction(const AutofillPrivateExtensionFunction&) =
      delete;
  AutofillPrivateExtensionFunction& operator=(
      const AutofillPrivateExtensionFunction&) = delete;

 protected:
  ~AutofillPrivateExtensionFunction() override = default;

  autofill::AddressDataManager* address_data_manager();
  autofill::ContentAutofillClient* autofill_client();
  autofill::PaymentsDataManager* payments_data_manager();
};

class AutofillPrivateGetAccountInfoFunction
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateSaveAddressFunction
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateRemoveAddressFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateRemoveAddressFunction() = default;
  AutofillPrivateRemoveAddressFunction(
      const AutofillPrivateRemoveAddressFunction&) = delete;
  AutofillPrivateRemoveAddressFunction& operator=(
      const AutofillPrivateRemoveAddressFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.removeAddress",
                             AUTOFILLPRIVATE_REMOVEADDRESS)

 protected:
  ~AutofillPrivateRemoveAddressFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetCountryListFunction
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateGetAddressComponentsFunction
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateGetAddressListFunction
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateSaveCreditCardFunction
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateRemovePaymentsEntityFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateRemovePaymentsEntityFunction() = default;
  AutofillPrivateRemovePaymentsEntityFunction(
      const AutofillPrivateRemovePaymentsEntityFunction&) = delete;
  AutofillPrivateRemovePaymentsEntityFunction& operator=(
      const AutofillPrivateRemovePaymentsEntityFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.removePaymentsEntity",
                             AUTOFILLPRIVATE_REMOVEPAYMENTSENTITY)

 protected:
  ~AutofillPrivateRemovePaymentsEntityFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetCreditCardListFunction
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateLogServerCardLinkClickedFunction
    : public AutofillPrivateExtensionFunction {
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
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateSaveIbanFunction
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateGetIbanListFunction
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateIsValidIbanFunction
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateAddVirtualCardFunction
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateRemoveVirtualCardFunction
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateGetPayOverTimeIssuerListFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateGetPayOverTimeIssuerListFunction() = default;
  AutofillPrivateGetPayOverTimeIssuerListFunction(
      const AutofillPrivateGetPayOverTimeIssuerListFunction&) = delete;
  AutofillPrivateGetPayOverTimeIssuerListFunction& operator=(
      const AutofillPrivateGetPayOverTimeIssuerListFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getPayOverTimeIssuerList",
                             AUTOFILLPRIVATE_GETPAYOVERTIMEISSUERLIST)

 protected:
  ~AutofillPrivateGetPayOverTimeIssuerListFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction
    : public AutofillPrivateExtensionFunction {
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
class AutofillPrivateGetLocalCardFunction
    : public AutofillPrivateExtensionFunction {
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
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateBulkDeleteAllCvcsFunction
    : public AutofillPrivateExtensionFunction {
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
    : public AutofillPrivateExtensionFunction {
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

class AutofillPrivateAddOrUpdateEntityInstanceFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateAddOrUpdateEntityInstanceFunction() = default;
  AutofillPrivateAddOrUpdateEntityInstanceFunction(
      const AutofillPrivateAddOrUpdateEntityInstanceFunction&) = delete;
  AutofillPrivateAddOrUpdateEntityInstanceFunction& operator=(
      const AutofillPrivateAddOrUpdateEntityInstanceFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.addOrUpdateEntityInstance",
                             AUTOFILLPRIVATE_ADDORUPDATEENTITYINSTANCE)

 protected:
  ~AutofillPrivateAddOrUpdateEntityInstanceFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateRemoveEntityInstanceFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateRemoveEntityInstanceFunction() = default;
  AutofillPrivateRemoveEntityInstanceFunction(
      const AutofillPrivateRemoveEntityInstanceFunction&) = delete;
  AutofillPrivateRemoveEntityInstanceFunction& operator=(
      const AutofillPrivateRemoveEntityInstanceFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.removeEntityInstance",
                             AUTOFILLPRIVATE_REMOVEENTITYINSTANCE)

 protected:
  ~AutofillPrivateRemoveEntityInstanceFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateLoadEntityInstancesFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateLoadEntityInstancesFunction() = default;
  AutofillPrivateLoadEntityInstancesFunction(
      const AutofillPrivateLoadEntityInstancesFunction&) = delete;
  AutofillPrivateLoadEntityInstancesFunction& operator=(
      const AutofillPrivateLoadEntityInstancesFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.loadEntityInstances",
                             AUTOFILLPRIVATE_LOADENTITYINSTANCES)

 protected:
  ~AutofillPrivateLoadEntityInstancesFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetEntityInstanceByGuidFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateGetEntityInstanceByGuidFunction() = default;
  AutofillPrivateGetEntityInstanceByGuidFunction(
      const AutofillPrivateGetEntityInstanceByGuidFunction&) = delete;
  AutofillPrivateGetEntityInstanceByGuidFunction& operator=(
      const AutofillPrivateGetEntityInstanceByGuidFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getEntityInstanceByGuid",
                             AUTOFILLPRIVATE_GETENTITYINSTANCEBYGUID)

 protected:
  ~AutofillPrivateGetEntityInstanceByGuidFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetWritableEntityTypesFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateGetWritableEntityTypesFunction() = default;
  AutofillPrivateGetWritableEntityTypesFunction(
      const AutofillPrivateGetWritableEntityTypesFunction&) = delete;
  AutofillPrivateGetWritableEntityTypesFunction& operator=(
      const AutofillPrivateGetWritableEntityTypesFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getWritableEntityTypes",
                             AUTOFILLPRIVATE_GETWRITABLEENTITYTYPES)

 protected:
  ~AutofillPrivateGetWritableEntityTypesFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetAllAttributeTypesForEntityTypeNameFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateGetAllAttributeTypesForEntityTypeNameFunction() = default;
  AutofillPrivateGetAllAttributeTypesForEntityTypeNameFunction(
      const AutofillPrivateGetAllAttributeTypesForEntityTypeNameFunction&) =
      delete;
  AutofillPrivateGetAllAttributeTypesForEntityTypeNameFunction& operator=(
      const AutofillPrivateGetAllAttributeTypesForEntityTypeNameFunction&) =
      delete;
  DECLARE_EXTENSION_FUNCTION(
      "autofillPrivate.getAllAttributeTypesForEntityTypeName",
      AUTOFILLPRIVATE_GETALLATTRIBUTETYPESFORENTITYTYPENAME)

 protected:
  ~AutofillPrivateGetAllAttributeTypesForEntityTypeNameFunction() override =
      default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetAutofillAiOptInStatusFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateGetAutofillAiOptInStatusFunction() = default;
  AutofillPrivateGetAutofillAiOptInStatusFunction(
      const AutofillPrivateGetAutofillAiOptInStatusFunction&) = delete;
  AutofillPrivateGetAutofillAiOptInStatusFunction& operator=(
      const AutofillPrivateGetAutofillAiOptInStatusFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getAutofillAiOptInStatus",
                             AUTOFILLPRIVATE_GETAUTOFILLAIOPTINSTATUS)

 protected:
  ~AutofillPrivateGetAutofillAiOptInStatusFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateSetAutofillAiOptInStatusFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateSetAutofillAiOptInStatusFunction() = default;
  AutofillPrivateSetAutofillAiOptInStatusFunction(
      const AutofillPrivateSetAutofillAiOptInStatusFunction&) = delete;
  AutofillPrivateSetAutofillAiOptInStatusFunction& operator=(
      const AutofillPrivateSetAutofillAiOptInStatusFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.setAutofillAiOptInStatus",
                             AUTOFILLPRIVATE_SETAUTOFILLAIOPTINSTATUS)

 protected:
  ~AutofillPrivateSetAutofillAiOptInStatusFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetWalletablePassDetectionOptInStatusFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateGetWalletablePassDetectionOptInStatusFunction() = default;
  AutofillPrivateGetWalletablePassDetectionOptInStatusFunction(
      const AutofillPrivateGetWalletablePassDetectionOptInStatusFunction&) =
      delete;
  AutofillPrivateGetWalletablePassDetectionOptInStatusFunction& operator=(
      const AutofillPrivateGetWalletablePassDetectionOptInStatusFunction&) =
      delete;
  DECLARE_EXTENSION_FUNCTION(
      "autofillPrivate.getWalletablePassDetectionOptInStatus",
      AUTOFILLPRIVATE_GETWALLETABLEPASSDETECTIONOPTINSTATUS)

 protected:
  ~AutofillPrivateGetWalletablePassDetectionOptInStatusFunction() override =
      default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateSetWalletablePassDetectionOptInStatusFunction
    : public AutofillPrivateExtensionFunction {
 public:
  AutofillPrivateSetWalletablePassDetectionOptInStatusFunction() = default;
  AutofillPrivateSetWalletablePassDetectionOptInStatusFunction(
      const AutofillPrivateSetWalletablePassDetectionOptInStatusFunction&) =
      delete;
  AutofillPrivateSetWalletablePassDetectionOptInStatusFunction& operator=(
      const AutofillPrivateSetWalletablePassDetectionOptInStatusFunction&) =
      delete;
  DECLARE_EXTENSION_FUNCTION(
      "autofillPrivate.setWalletablePassDetectionOptInStatus",
      AUTOFILLPRIVATE_SETWALLETABLEPASSDETECTIONOPTINSTATUS)

 protected:
  ~AutofillPrivateSetWalletablePassDetectionOptInStatusFunction() override =
      default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_API_H_
