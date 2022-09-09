// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace ash {
class DeskTemplate;
class Desk;
}

namespace extensions {
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

class WmDesksPrivateSaveActiveDeskFunction : public ExtensionFunction {
 public:
  WmDesksPrivateSaveActiveDeskFunction();
  WmDesksPrivateSaveActiveDeskFunction(
      const WmDesksPrivateSaveActiveDeskFunction&) = delete;
  WmDesksPrivateSaveActiveDeskFunction& operator=(
      const WmDesksPrivateSaveActiveDeskFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.saveActiveDesk",
                             WMDESKSPRIVATE_SAVEACTIVEDESK)

 protected:
  ~WmDesksPrivateSaveActiveDeskFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnSavedActiveDesk(std::unique_ptr<ash::DeskTemplate> desk_template,
                         std::string error_string);
};

class WmDesksPrivateDeleteSavedDeskFunction : public ExtensionFunction {
 public:
  WmDesksPrivateDeleteSavedDeskFunction();
  WmDesksPrivateDeleteSavedDeskFunction(
      const WmDesksPrivateDeleteSavedDeskFunction&) = delete;
  WmDesksPrivateDeleteSavedDeskFunction& operator=(
      const WmDesksPrivateDeleteSavedDeskFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.deleteSavedDesk",
                             WMDESKSPRIVATE_DELETESAVEDDESK)

 protected:
  ~WmDesksPrivateDeleteSavedDeskFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnDeletedSavedDesk(std::string error_string);
};

class WmDesksPrivateRecallSavedDeskFunction : public ExtensionFunction {
 public:
  WmDesksPrivateRecallSavedDeskFunction();
  WmDesksPrivateRecallSavedDeskFunction(
      const WmDesksPrivateRecallSavedDeskFunction&) = delete;
  WmDesksPrivateRecallSavedDeskFunction& operator=(
      const WmDesksPrivateRecallSavedDeskFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.recallSavedDesk",
                             WMDESKSPRIVATE_RECALLSAVEDDESK)

 protected:
  ~WmDesksPrivateRecallSavedDeskFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnRecalledSavedDesk(std::string error_string, const base::GUID& desk_Id);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_API_H_
