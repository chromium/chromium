// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_ECHO_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_ECHO_PRIVATE_API_H_

#include <string>

#include "base/types/expected.h"
#include "base/values.h"
#include "extensions/browser/extension_function.h"

class PrefRegistrySimple;

namespace chromeos {

namespace echo_offer {

// Registers the EchoCheckedOffers field in Local State.
void RegisterPrefs(PrefRegistrySimple* registry);

// Removes nested empty dictionaries from |dict|.
void RemoveEmptyValueDicts(base::Value::Dict& dict);

}  // namespace echo_offer

}  // namespace chromeos

class EchoPrivateGetRegistrationCodeFunction : public ExtensionFunction {
 public:
  EchoPrivateGetRegistrationCodeFunction();

 protected:
  ~EchoPrivateGetRegistrationCodeFunction() override;
  ResponseAction Run() override;

 private:
  void RespondWithResult(const std::string& result);
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getRegistrationCode",
                             ECHOPRIVATE_GETREGISTRATIONCODE)
};

class EchoPrivateGetOobeTimestampFunction : public ExtensionFunction {
 public:
  EchoPrivateGetOobeTimestampFunction();

 protected:
  ~EchoPrivateGetOobeTimestampFunction() override;
  ResponseAction Run() override;

 private:
  void RespondWithResult(std::optional<base::Time> timestamp);

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
class EchoPrivateGetUserConsentFunction : public ExtensionFunction {
 public:
  EchoPrivateGetUserConsentFunction();

 protected:
  ~EchoPrivateGetUserConsentFunction() override;
  ResponseAction Run() override;

 private:
  // Sets result and calls SendResponse.
  void Finalize(bool consent);

  DECLARE_EXTENSION_FUNCTION("echoPrivate.getUserConsent",
                             ECHOPRIVATE_GETUSERCONSENT)
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_ECHO_PRIVATE_API_H_
