// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_API_H_

#include "base/uuid.h"
#include "base/values.h"
#include "chrome/common/extensions/api/wm_desks_private.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class WmDesksPrivateGetSavedDesksFunction : public ExtensionFunction {
 public:
  WmDesksPrivateGetSavedDesksFunction();
  WmDesksPrivateGetSavedDesksFunction(
      const WmDesksPrivateGetSavedDesksFunction&) = delete;
  WmDesksPrivateGetSavedDesksFunction& operator=(
      const WmDesksPrivateGetSavedDesksFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.getSavedDesks",
                             WMDESKSPRIVATE_GETSAVEDDESKS)

 protected:
  ~WmDesksPrivateGetSavedDesksFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetSavedDesks(std::string error_string,
                       std::vector<api::wm_desks_private::SavedDesk> desks);
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
  void OnGetDeskTemplateJson(std::string error, base::Value template_json);
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
  void OnLaunchDesk(std::string error, const base::Uuid& desk_uuid);
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

  void OnRemoveDesk(std::string error);
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
  void OnGetAllDesks(std::string error,
                     std::vector<api::wm_desks_private::Desk> desks);
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

  void OnSetWindowProperties(std::string error);
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
  void OnSavedActiveDesk(std::string error,
                         api::wm_desks_private::SavedDesk desk);
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
  void OnDeletedSavedDesk(std::string error);
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

  void OnRecalledSavedDesk(std::string error, const base::Uuid& desk_Id);
};

class WmDesksPrivateGetActiveDeskFunction : public ExtensionFunction {
 public:
  WmDesksPrivateGetActiveDeskFunction();
  WmDesksPrivateGetActiveDeskFunction(
      const WmDesksPrivateGetActiveDeskFunction&) = delete;
  WmDesksPrivateGetActiveDeskFunction& operator=(
      const WmDesksPrivateGetActiveDeskFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.getActiveDesk",
                             WMDESKSPRIVATE_GETACTIVEDESK)

 protected:
  ~WmDesksPrivateGetActiveDeskFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetActiveDesk(std::string error_string, const base::Uuid& desk_Id);
};

class WmDesksPrivateSwitchDeskFunction : public ExtensionFunction {
 public:
  WmDesksPrivateSwitchDeskFunction();
  WmDesksPrivateSwitchDeskFunction(const WmDesksPrivateSwitchDeskFunction&) =
      delete;
  WmDesksPrivateSwitchDeskFunction& operator=(
      const WmDesksPrivateSwitchDeskFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.switchDesk",
                             WMDESKSPRIVATE_SWITCHDESK)

 protected:
  ~WmDesksPrivateSwitchDeskFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnSwitchDesk(std::string error_string);
};

class WmDesksPrivateGetDeskByIDFunction : public ExtensionFunction {
 public:
  WmDesksPrivateGetDeskByIDFunction();
  WmDesksPrivateGetDeskByIDFunction(const WmDesksPrivateGetDeskByIDFunction&) =
      delete;
  WmDesksPrivateGetDeskByIDFunction& operator=(
      const WmDesksPrivateGetDeskByIDFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("wmDesksPrivate.getDeskByID",
                             WMDESKSPRIVATE_GETDESKBYID)

 protected:
  ~WmDesksPrivateGetDeskByIDFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetDeskByID(std::string error_string,
                     api::wm_desks_private::Desk desk);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_API_H_
