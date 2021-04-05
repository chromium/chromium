// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_API_H_

#include <string>

#include "extensions/browser/extension_function.h"

namespace extensions {

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

class AutofillPrivateValidatePhoneNumbersFunction : public ExtensionFunction {
 public:
  AutofillPrivateValidatePhoneNumbersFunction() = default;
  AutofillPrivateValidatePhoneNumbersFunction(
      const AutofillPrivateValidatePhoneNumbersFunction&) = delete;
  AutofillPrivateValidatePhoneNumbersFunction& operator=(
      const AutofillPrivateValidatePhoneNumbersFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.validatePhoneNumbers",
                             AUTOFILLPRIVATE_VALIDATEPHONENUMBERS)

 protected:
  ~AutofillPrivateValidatePhoneNumbersFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateMaskCreditCardFunction : public ExtensionFunction {
 public:
  AutofillPrivateMaskCreditCardFunction() = default;
  AutofillPrivateMaskCreditCardFunction(
      const AutofillPrivateMaskCreditCardFunction&) = delete;
  AutofillPrivateMaskCreditCardFunction& operator=(
      const AutofillPrivateMaskCreditCardFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.maskCreditCard",
                             AUTOFILLPRIVATE_MASKCREDITCARD)

 protected:
  ~AutofillPrivateMaskCreditCardFunction() override = default;

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

class AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction
    : public ExtensionFunction {
 public:
  AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction() = default;
  AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction(
      const AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction&) = delete;
  AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction& operator=(
      const AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION(
      "autofillPrivate.setCreditCardFIDOAuthEnabledState",
      AUTOFILLPRIVATE_SETCREDITCARDFIDOAUTHENABLEDSTATE)

 protected:
  ~AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction() override =
      default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class AutofillPrivateGetUpiIdListFunction : public ExtensionFunction {
 public:
  AutofillPrivateGetUpiIdListFunction() = default;
  AutofillPrivateGetUpiIdListFunction(
      const AutofillPrivateGetUpiIdListFunction&) = delete;
  AutofillPrivateGetUpiIdListFunction& operator=(
      const AutofillPrivateGetUpiIdListFunction&) = delete;
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getUpiIdList",
                             AUTOFILLPRIVATE_GETUPIIDLIST)

 protected:
  ~AutofillPrivateGetUpiIdListFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_API_H_
