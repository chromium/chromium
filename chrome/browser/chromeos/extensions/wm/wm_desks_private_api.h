// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/quota_service.h"

namespace ash {
class DeskTemplate;
class Desk;
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
      const std::vector<const ash::DeskTemplate*>& desk_templates,
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

class WmDesksPrivateLaunchDeskFunction : public ExtensionFunction {
 public:
  WmDesksPrivateLaunchDeskFunction();
  WmDesksPrivateLaunchDeskFunction(const WmDesksPrivateLaunchDeskFunction&) =
      delete;
  WmDesksPrivateLaunchDeskFunction& operator=(
      const WmDesksPrivateLaunchDeskFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.launchDesk",
                             WMDESKSPRIVATE_LAUNCHDESK)

 protected:
  ~WmDesksPrivateLaunchDeskFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnLaunchDesk(std::string error_string, const base::GUID& desk_Id);
};

class WmDesksPrivateRemoveDeskFunction : public ExtensionFunction {
 public:
  WmDesksPrivateRemoveDeskFunction();
  WmDesksPrivateRemoveDeskFunction(const WmDesksPrivateRemoveDeskFunction&) =
      delete;
  WmDesksPrivateRemoveDeskFunction& operator=(
      const WmDesksPrivateRemoveDeskFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.removeDesk",
                             WMDESKSPRIVATE_REMOVEDESK)
 protected:
  ~WmDesksPrivateRemoveDeskFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnRemoveDesk(std::string error_string);
};

class WmDesksPrivateGetAllDesksFunction : public ExtensionFunction {
 public:
  WmDesksPrivateGetAllDesksFunction();
  WmDesksPrivateGetAllDesksFunction(const WmDesksPrivateGetAllDesksFunction&) =
      delete;
  WmDesksPrivateGetAllDesksFunction& operator=(
      const WmDesksPrivateGetAllDesksFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.getAllDesks",
                             WMDESKSPRIVATE_GETALLDESKS)

 protected:
  ~WmDesksPrivateGetAllDesksFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetAllDesks(const std::vector<const ash::Desk*>& desks,
                     std::string error_string);
};

class WmDesksPrivateSetWindowPropertiesFunction : public ExtensionFunction {
 public:
  WmDesksPrivateSetWindowPropertiesFunction();
  WmDesksPrivateSetWindowPropertiesFunction(
      const WmDesksPrivateSetWindowPropertiesFunction&) = delete;
  WmDesksPrivateSetWindowPropertiesFunction& operator=(
      const WmDesksPrivateSetWindowPropertiesFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.setWindowProperties",
                             WMDESKSPRIVATE_SETWINDOWPROPERTIES)

 protected:
  ~WmDesksPrivateSetWindowPropertiesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnSetWindowProperties(std::string error_string);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_API_H_
