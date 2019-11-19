// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_CAST_PRIVATE_NETWORKING_CAST_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_CAST_PRIVATE_NETWORKING_CAST_PRIVATE_API_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace base {
class DictionaryValue;
}

namespace extensions {

class NetworkingCastPrivateVerifyDestinationFunction
    : public ExtensionFunction {
 public:
  NetworkingCastPrivateVerifyDestinationFunction() {}
  DECLARE_EXTENSION_FUNCTION("networking.castPrivate.verifyDestination",
                             NETWORKINGCASTPRIVATE_VERIFYDESTINATION)

 protected:
  ~NetworkingCastPrivateVerifyDestinationFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void Success(bool result);
  void Failure(const std::string& error);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingCastPrivateVerifyDestinationFunction);
};

class NetworkingCastPrivateVerifyAndEncryptDataFunction
    : public ExtensionFunction {
 public:
  NetworkingCastPrivateVerifyAndEncryptDataFunction() {}
  DECLARE_EXTENSION_FUNCTION("networking.castPrivate.verifyAndEncryptData",
                             NETWORKINGCASTPRIVATE_VERIFYANDENCRYPTDATA)

 protected:
  ~NetworkingCastPrivateVerifyAndEncryptDataFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void Success(const std::string& result);
  void Failure(const std::string& error);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingCastPrivateVerifyAndEncryptDataFunction);
};

class NetworkingCastPrivateSetWifiTDLSEnabledStateFunction
    : public ExtensionFunction {
 public:
  NetworkingCastPrivateSetWifiTDLSEnabledStateFunction() {}
  DECLARE_EXTENSION_FUNCTION("networking.castPrivate.setWifiTDLSEnabledState",
                             NETWORKINGCASTPRIVATE_SETWIFITDLSENABLEDSTATE)

 protected:
  ~NetworkingCastPrivateSetWifiTDLSEnabledStateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

#if defined(OS_CHROMEOS)
  void Success(const std::string& result);
  void Failure(const std::string& error,
               std::unique_ptr<base::DictionaryValue> error_data);
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(
      NetworkingCastPrivateSetWifiTDLSEnabledStateFunction);
};

class NetworkingCastPrivateGetWifiTDLSStatusFunction
    : public ExtensionFunction {
 public:
  NetworkingCastPrivateGetWifiTDLSStatusFunction() {}
  DECLARE_EXTENSION_FUNCTION("networking.castPrivate.getWifiTDLSStatus",
                             NETWORKINGCASTPRIVATE_GETWIFITDLSSTATUS)

 protected:
  ~NetworkingCastPrivateGetWifiTDLSStatusFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

#if defined(OS_CHROMEOS)
  void Success(const std::string& result);
  void Failure(const std::string& error,
               std::unique_ptr<base::DictionaryValue> error_data);
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingCastPrivateGetWifiTDLSStatusFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_CAST_PRIVATE_NETWORKING_CAST_PRIVATE_API_H_
