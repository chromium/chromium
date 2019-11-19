// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/ui/echo_dialog_listener.h"
#include "chrome/browser/extensions/chrome_extension_function.h"

class PrefRegistrySimple;

namespace chromeos {

class EchoDialogView;

// Namespace to register the EchoCheckedOffers field in Local State.
namespace echo_offer {

void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace echo_offer

}  // namespace chromeos

class EchoPrivateGetRegistrationCodeFunction : public ExtensionFunction {
 public:
  EchoPrivateGetRegistrationCodeFunction();

 protected:
  ~EchoPrivateGetRegistrationCodeFunction() override;
  ResponseAction Run() override;

 private:
  ResponseValue GetRegistrationCode(const std::string& type);
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getRegistrationCode",
                             ECHOPRIVATE_GETREGISTRATIONCODE)
};

class EchoPrivateGetOobeTimestampFunction
    : public ChromeAsyncExtensionFunction {
 public:
  EchoPrivateGetOobeTimestampFunction();

 protected:
  ~EchoPrivateGetOobeTimestampFunction() override;
  bool RunAsync() override;

 private:
  bool GetOobeTimestampOnFileSequence();
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getOobeTimestamp",
                             ECHOPRIVATE_GETOOBETIMESTAMP)
};

class EchoPrivateSetOfferInfoFunction : public ExtensionFunction {
 public:
  EchoPrivateSetOfferInfoFunction();

 protected:
  ~EchoPrivateSetOfferInfoFunction() override;
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("echoPrivate.setOfferInfo",
                             ECHOPRIVATE_SETOFFERINFO)
};

class EchoPrivateGetOfferInfoFunction : public ExtensionFunction {
 public:
  EchoPrivateGetOfferInfoFunction();

 protected:
  ~EchoPrivateGetOfferInfoFunction() override;
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getOfferInfo",
                             ECHOPRIVATE_GETOFFERINFO)
};

// The function first checks if offers redeeming is allowed by the device
// policy. It should then show a dialog that, depending on the check result,
// either asks user's consent to verify the device's eligibility for the offer,
// or informs the user that the offers redeeming is disabled.
// It returns whether the user consent was given.
class EchoPrivateGetUserConsentFunction : public ChromeAsyncExtensionFunction,
                                          public chromeos::EchoDialogListener {
 public:
  // Type for the dialog shown callback used in tests.
  using DialogShownTestCallback =
      base::RepeatingCallback<void(chromeos::EchoDialogView* dialog)>;

  EchoPrivateGetUserConsentFunction();

  // Creates the function with non-null dialog_shown_callback_.
  // To be used in tests.
  static scoped_refptr<EchoPrivateGetUserConsentFunction> CreateForTest(
      const DialogShownTestCallback& dialog_shown_callback);

 protected:
  ~EchoPrivateGetUserConsentFunction() override;

  bool RunAsync() override;

 private:
  // chromeos::EchoDialogListener overrides.
  void OnAccept() override;
  void OnCancel() override;
  void OnMoreInfoLinkClicked() override;

  // Checks whether "allow redeem ChromeOS registration offers" setting is
  // disabled in cros settings. It may be asynchronous if the needed settings
  // provider is not yet trusted.
  // Upon completion |OnRedeemOffersAllowed| is called.
  void CheckRedeemOffersAllowed();
  void OnRedeemOffersAllowedChecked(bool is_allowed);

  // Sets result and calls SendResponse.
  void Finalize(bool consent);

  // Result of |CheckRedeemOffersAllowed()|.
  bool redeem_offers_allowed_;
  // Callback used in tests. Called after an echo dialog is shown.
  DialogShownTestCallback dialog_shown_callback_;

  DECLARE_EXTENSION_FUNCTION("echoPrivate.getUserConsent",
                             ECHOPRIVATE_GETUSERCONSENT)
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_
