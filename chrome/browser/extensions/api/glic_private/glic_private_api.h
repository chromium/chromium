// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_GLIC_PRIVATE_GLIC_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_GLIC_PRIVATE_GLIC_PRIVATE_API_H_

#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/common/extensions/api/glic_private.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class GlicPrivateGetStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("glicPrivate.getState", GLICPRIVATE_GETSTATE)

  GlicPrivateGetStateFunction();
  GlicPrivateGetStateFunction(const GlicPrivateGetStateFunction&) = delete;
  GlicPrivateGetStateFunction& operator=(const GlicPrivateGetStateFunction&) =
      delete;

 protected:
  ~GlicPrivateGetStateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class GlicPrivateInvokeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("glicPrivate.invoke", GLICPRIVATE_INVOKE)

  GlicPrivateInvokeFunction();
  GlicPrivateInvokeFunction(const GlicPrivateInvokeFunction&) = delete;
  GlicPrivateInvokeFunction& operator=(const GlicPrivateInvokeFunction&) =
      delete;

 protected:
  ~GlicPrivateInvokeFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ResponseValue GetPromptResponseValueAndLog(
      extensions::api::glic_private::ErrorCode result);

  void OnPromptRetrieved(glic::GlicInvokeOptions options,
                         bool in_new_tab,
                         const std::string& document_id,
                         extensions::api::glic_private::ErrorCode result,
                         std::optional<std::string> prompt);
};

class GlicPrivateHasConversationFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("glicPrivate.hasConversation",
                             GLICPRIVATE_HASCONVERSATION)

  GlicPrivateHasConversationFunction();
  GlicPrivateHasConversationFunction(
      const GlicPrivateHasConversationFunction&) = delete;
  GlicPrivateHasConversationFunction& operator=(
      const GlicPrivateHasConversationFunction&) = delete;

 protected:
  ~GlicPrivateHasConversationFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class GlicPrivateActivateTabWithConversationFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("glicPrivate.activateTabWithConversation",
                             GLICPRIVATE_ACTIVATETABWITHCONVERSATION)

  GlicPrivateActivateTabWithConversationFunction();
  GlicPrivateActivateTabWithConversationFunction(
      const GlicPrivateActivateTabWithConversationFunction&) = delete;
  GlicPrivateActivateTabWithConversationFunction& operator=(
      const GlicPrivateActivateTabWithConversationFunction&) = delete;

 protected:
  ~GlicPrivateActivateTabWithConversationFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GLIC_PRIVATE_GLIC_PRIVATE_API_H_
