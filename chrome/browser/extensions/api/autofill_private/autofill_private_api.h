// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_API_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class AutofillPrivateSaveAddressFunction : public ExtensionFunction {
 public:
  AutofillPrivateSaveAddressFunction();
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.saveAddress",
                             AUTOFILLPRIVATE_SAVEADDRESS)

 protected:
  ~AutofillPrivateSaveAddressFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateSaveAddressFunction);
};

class AutofillPrivateGetCountryListFunction : public ExtensionFunction {
 public:
  AutofillPrivateGetCountryListFunction();
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getCountryList",
                             AUTOFILLPRIVATE_GETCOUNTRYLIST)

 protected:
  ~AutofillPrivateGetCountryListFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateGetCountryListFunction);
};

class AutofillPrivateGetAddressComponentsFunction : public ExtensionFunction {
 public:
  AutofillPrivateGetAddressComponentsFunction() {}
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getAddressComponents",
                             AUTOFILLPRIVATE_GETADDRESSCOMPONENTS)

 protected:
  ~AutofillPrivateGetAddressComponentsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateGetAddressComponentsFunction);
};

class AutofillPrivateGetAddressListFunction : public ExtensionFunction {
 public:
  AutofillPrivateGetAddressListFunction();
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getAddressList",
                             AUTOFILLPRIVATE_GETADDRESSLIST)

 protected:
  ~AutofillPrivateGetAddressListFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateGetAddressListFunction);
};

class AutofillPrivateSaveCreditCardFunction : public ExtensionFunction {
 public:
  AutofillPrivateSaveCreditCardFunction();
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.saveCreditCard",
                             AUTOFILLPRIVATE_SAVECREDITCARD)

 protected:
  ~AutofillPrivateSaveCreditCardFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateSaveCreditCardFunction);
};

class AutofillPrivateRemoveEntryFunction : public ExtensionFunction {
 public:
  AutofillPrivateRemoveEntryFunction();
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.removeEntry",
                             AUTOFILLPRIVATE_REMOVEENTRY)

 protected:
  ~AutofillPrivateRemoveEntryFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateRemoveEntryFunction);
};

class AutofillPrivateValidatePhoneNumbersFunction : public ExtensionFunction {
 public:
  AutofillPrivateValidatePhoneNumbersFunction() {}
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.validatePhoneNumbers",
                             AUTOFILLPRIVATE_VALIDATEPHONENUMBERS)

 protected:
  ~AutofillPrivateValidatePhoneNumbersFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateValidatePhoneNumbersFunction);
};

class AutofillPrivateMaskCreditCardFunction : public ExtensionFunction {
 public:
  AutofillPrivateMaskCreditCardFunction();
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.maskCreditCard",
                             AUTOFILLPRIVATE_MASKCREDITCARD)

 protected:
  ~AutofillPrivateMaskCreditCardFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateMaskCreditCardFunction);
};

class AutofillPrivateGetCreditCardListFunction : public ExtensionFunction {
 public:
  AutofillPrivateGetCreditCardListFunction();
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.getCreditCardList",
                             AUTOFILLPRIVATE_GETCREDITCARDLIST)

 protected:
  ~AutofillPrivateGetCreditCardListFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateGetCreditCardListFunction);
};

class AutofillPrivateMigrateCreditCardsFunction : public ExtensionFunction {
 public:
  AutofillPrivateMigrateCreditCardsFunction();
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.migrateCreditCards",
                             AUTOFILLPRIVATE_MIGRATECREDITCARDS)

 protected:
  ~AutofillPrivateMigrateCreditCardsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateMigrateCreditCardsFunction);
};

class AutofillPrivateLogServerCardLinkClickedFunction
    : public ExtensionFunction {
 public:
  AutofillPrivateLogServerCardLinkClickedFunction();
  DECLARE_EXTENSION_FUNCTION("autofillPrivate.logServerCardLinkClicked",
                             AUTOFILLPRIVATE_SERVERCARDLINKCLICKED)

 protected:
  ~AutofillPrivateLogServerCardLinkClickedFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateLogServerCardLinkClickedFunction);
};

class AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction
    : public ExtensionFunction {
 public:
  AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction();
  DECLARE_EXTENSION_FUNCTION(
      "autofillPrivate.setCreditCardFIDOAuthEnabledState",
      AUTOFILLPRIVATE_SETCREDITCARDFIDOAUTHENABLEDSTATE)

 protected:
  ~AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(
      AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_API_H_
