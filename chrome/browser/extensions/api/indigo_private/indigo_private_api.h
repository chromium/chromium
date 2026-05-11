// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INDIGO_PRIVATE_INDIGO_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_INDIGO_PRIVATE_INDIGO_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class IndigoPrivateReadyToRenderFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("indigoPrivate.readyToRender",
                             INDIGOPRIVATE_READYTORENDER)

  IndigoPrivateReadyToRenderFunction();
  IndigoPrivateReadyToRenderFunction(
      const IndigoPrivateReadyToRenderFunction&) = delete;
  IndigoPrivateReadyToRenderFunction& operator=(
      const IndigoPrivateReadyToRenderFunction&) = delete;

 protected:
  ~IndigoPrivateReadyToRenderFunction() override;

  // Override from ExtensionFunction:
  ResponseAction Run() override;
};

class IndigoPrivateGetOriginalImageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("indigoPrivate.getOriginalImage",
                             INDIGOPRIVATE_GETORIGINALIMAGE)

  IndigoPrivateGetOriginalImageFunction();
  IndigoPrivateGetOriginalImageFunction(
      const IndigoPrivateGetOriginalImageFunction&) = delete;
  IndigoPrivateGetOriginalImageFunction& operator=(
      const IndigoPrivateGetOriginalImageFunction&) = delete;

 protected:
  ~IndigoPrivateGetOriginalImageFunction() override;

  // Override from ExtensionFunction:
  ResponseAction Run() override;
};

class IndigoPrivateGetReplacementImageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("indigoPrivate.getReplacementImage",
                             INDIGOPRIVATE_GETREPLACEMENTIMAGE)

  IndigoPrivateGetReplacementImageFunction();
  IndigoPrivateGetReplacementImageFunction(
      const IndigoPrivateGetReplacementImageFunction&) = delete;
  IndigoPrivateGetReplacementImageFunction& operator=(
      const IndigoPrivateGetReplacementImageFunction&) = delete;

 protected:
  ~IndigoPrivateGetReplacementImageFunction() override;

  // Override from ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnReplacementImageAvailable(const GURL& replacement_image_url);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INDIGO_PRIVATE_INDIGO_PRIVATE_API_H_
