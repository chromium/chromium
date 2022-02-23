// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

namespace ash {
class DeskTemplate;
}

namespace extensions {

class WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction
    : public ExtensionFunction {
 public:
  WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction();
  WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction(
      const WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction&) = delete;
  WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction& operator=(
      const WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.captureActiveDeskAndSaveTemplate",
                             WMDESKSPRIVATE_CAPTUREACTIVEDESKANDSAVETEMPLATE)

 protected:
  ~WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnCaptureActiveDeskAndSaveTemplateCompleted(
      std::unique_ptr<ash::DeskTemplate> desk_template,
      std::string error_string);
};

class WmDesksPrivateUpdateDeskTemplateFunction : public ExtensionFunction {
 public:
  WmDesksPrivateUpdateDeskTemplateFunction();
  WmDesksPrivateUpdateDeskTemplateFunction(
      const WmDesksPrivateUpdateDeskTemplateFunction&) = delete;
  WmDesksPrivateUpdateDeskTemplateFunction& operator=(
      const WmDesksPrivateUpdateDeskTemplateFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.updateDeskTemplate",
                             WMDESKSPRIVATE_UPDATEDESKTEMPLATE)

 protected:
  ~WmDesksPrivateUpdateDeskTemplateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnUpdateDeskTemplateCompleted(std::string error_string);
};

class WmDesksPrivateGetSavedDeskTemplatesFunction : public ExtensionFunction {
 public:
  WmDesksPrivateGetSavedDeskTemplatesFunction();
  WmDesksPrivateGetSavedDeskTemplatesFunction(
      const WmDesksPrivateGetSavedDeskTemplatesFunction&) = delete;
  WmDesksPrivateGetSavedDeskTemplatesFunction& operator=(
      const WmDesksPrivateGetSavedDeskTemplatesFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.getSavedDeskTemplates",
                             WMDESKSPRIVATE_GETSAVEDDESKTEMPLATES)

 protected:
  ~WmDesksPrivateGetSavedDeskTemplatesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetSavedDeskTemplate(
      const std::vector<ash::DeskTemplate*>& desk_templates,
      std::string error_string);
};

class WmDesksPrivateGetDeskTemplateJsonFunction : public ExtensionFunction {
 public:
  WmDesksPrivateGetDeskTemplateJsonFunction();
  WmDesksPrivateGetDeskTemplateJsonFunction(
      const WmDesksPrivateGetDeskTemplateJsonFunction&) = delete;
  WmDesksPrivateGetDeskTemplateJsonFunction& operator=(
      const WmDesksPrivateGetDeskTemplateJsonFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.getDeskTemplateJson",
                             WMDESKSPRIVATE_GETDESKTEMPLATEJSON)

 protected:
  ~WmDesksPrivateGetDeskTemplateJsonFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetDeskTemplateJson(const std::string& template_json,
                             std::string error_string);
};

class WmDesksPrivateDeleteDeskTemplateFunction : public ExtensionFunction {
 public:
  WmDesksPrivateDeleteDeskTemplateFunction();
  WmDesksPrivateDeleteDeskTemplateFunction(
      const WmDesksPrivateDeleteDeskTemplateFunction&) = delete;
  WmDesksPrivateDeleteDeskTemplateFunction& operator=(
      const WmDesksPrivateDeleteDeskTemplateFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.deleteDeskTemplate",
                             WMDESKSPRIVATE_DELETEDESKTEMPLATE)

 protected:
  ~WmDesksPrivateDeleteDeskTemplateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnDeleteDeskTemplateCompleted(std::string error_string);
};

class WmDesksPrivateLaunchDeskTemplateFunction : public ExtensionFunction {
 public:
  WmDesksPrivateLaunchDeskTemplateFunction();
  WmDesksPrivateLaunchDeskTemplateFunction(
      const WmDesksPrivateLaunchDeskTemplateFunction&) = delete;
  WmDesksPrivateLaunchDeskTemplateFunction& operator=(
      const WmDesksPrivateLaunchDeskTemplateFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.launchDeskTemplate",
                             WMDESKSPRIVATE_LAUNCHDESKTEMPLATE)

 protected:
  ~WmDesksPrivateLaunchDeskTemplateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnLaunchDeskTemplate(std::string error_string);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_API_H_
