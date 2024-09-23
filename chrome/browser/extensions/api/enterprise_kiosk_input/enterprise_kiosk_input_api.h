// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_KIOSK_INPUT_ENTERPRISE_KIOSK_INPUT_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_KIOSK_INPUT_ENTERPRISE_KIOSK_INPUT_API_H_

#include "chromeos/crosapi/mojom/input_methods.mojom-forward.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class EnterpriseKioskInputSetCurrentInputMethodFunction
    : public ExtensionFunction {
 public:
  EnterpriseKioskInputSetCurrentInputMethodFunction();

  EnterpriseKioskInputSetCurrentInputMethodFunction(
      const EnterpriseKioskInputSetCurrentInputMethodFunction&) = delete;
  EnterpriseKioskInputSetCurrentInputMethodFunction& operator=(
      const EnterpriseKioskInputSetCurrentInputMethodFunction&) = delete;

 protected:
  ~EnterpriseKioskInputSetCurrentInputMethodFunction() override;

  ResponseAction Run() override;

 private:
  // Called asynchronously when crosapi returns the result.
  void OnChangeInputMethodDone(std::string input_method_id, bool succeeded);

  DECLARE_EXTENSION_FUNCTION("enterprise.kioskInput.setCurrentInputMethod",
                             ENTERPRISE_KIOSKINPUT_SETCURRENTINPUTMETHOD)
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_KIOSK_INPUT_ENTERPRISE_KIOSK_INPUT_API_H_
