// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api_stub.h"

#include "base/notimplemented.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "url/gurl.h"

namespace extensions {

namespace windows = api::windows;
namespace tabs = api::tabs;

namespace {

constexpr char kTabsNotImplemented[] = "chrome.tabs not implemented";
constexpr char kWindowsNotImplemented[] = "chrome.windows not implemented";

}  // namespace

// Windows ---------------------------------------------------------------------

ExtensionFunction::ResponseAction WindowsGetFunction::Run() {
  std::optional<windows::Get::Params> params =
      windows::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kWindowsNotImplemented));
}

ExtensionFunction::ResponseAction WindowsGetCurrentFunction::Run() {
  std::optional<windows::GetCurrent::Params> params =
      windows::GetCurrent::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kWindowsNotImplemented));
}

ExtensionFunction::ResponseAction WindowsGetLastFocusedFunction::Run() {
  std::optional<windows::GetLastFocused::Params> params =
      windows::GetLastFocused::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kWindowsNotImplemented));
}

ExtensionFunction::ResponseAction WindowsGetAllFunction::Run() {
  std::optional<windows::GetAll::Params> params =
      windows::GetAll::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kWindowsNotImplemented));
}

ExtensionFunction::ResponseAction WindowsCreateFunction::Run() {
  std::optional<windows::Create::Params> params =
      windows::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kWindowsNotImplemented));
}

ExtensionFunction::ResponseAction WindowsUpdateFunction::Run() {
  std::optional<windows::Update::Params> params =
      windows::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kWindowsNotImplemented));
}

ExtensionFunction::ResponseAction WindowsRemoveFunction::Run() {
  std::optional<windows::Remove::Params> params =
      windows::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kWindowsNotImplemented));
}

// Tabs ------------------------------------------------------------------------

ExtensionFunction::ResponseAction TabsGetSelectedFunction::Run() {
  std::optional<tabs::GetSelected::Params> params =
      tabs::GetSelected::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsGetAllInWindowFunction::Run() {
  std::optional<tabs::GetAllInWindow::Params> params =
      tabs::GetAllInWindow::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsQueryFunction::Run() {
  std::optional<tabs::Query::Params> params =
      tabs::Query::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  NOTIMPLEMENTED() << "Using stub implementation and returning active tab";
  base::Value::List result;
  api::tabs::Tab tab_object;
  // Always return the active tab in the current window.
  for (TabModel* tab_model : TabModelList::models()) {
    if (!tab_model->IsActiveModel()) {
      continue;
    }
    auto* web_contents = tab_model->GetActiveWebContents();
    if (!web_contents) {
      continue;
    }
    tab_object.id = ExtensionTabUtil::GetTabId(web_contents);
    result.Append(tab_object.ToValue());
    return RespondNow(WithArguments(std::move(result)));
  }
  return RespondNow(Error("No active tab"));
}

ExtensionFunction::ResponseAction TabsCreateFunction::Run() {
  std::optional<tabs::Create::Params> params =
      tabs::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsDuplicateFunction::Run() {
  std::optional<tabs::Duplicate::Params> params =
      tabs::Duplicate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsGetFunction::Run() {
  std::optional<tabs::Get::Params> params = tabs::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsGetCurrentFunction::Run() {
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsHighlightFunction::Run() {
  std::optional<tabs::Highlight::Params> params =
      tabs::Highlight::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

TabsUpdateFunction::TabsUpdateFunction() = default;

ExtensionFunction::ResponseAction TabsUpdateFunction::Run() {
  std::optional<tabs::Update::Params> params =
      tabs::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsMoveFunction::Run() {
  std::optional<tabs::Move::Params> params = tabs::Move::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsReloadFunction::Run() {
  std::optional<tabs::Reload::Params> params =
      tabs::Reload::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

TabsRemoveFunction::TabsRemoveFunction() = default;

ExtensionFunction::ResponseAction TabsRemoveFunction::Run() {
  std::optional<tabs::Remove::Params> params =
      tabs::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsGroupFunction::Run() {
  std::optional<tabs::Group::Params> params =
      tabs::Group::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsUngroupFunction::Run() {
  std::optional<tabs::Ungroup::Params> params =
      tabs::Ungroup::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

TabsCaptureVisibleTabFunction::TabsCaptureVisibleTabFunction() = default;

ExtensionFunction::ResponseAction TabsCaptureVisibleTabFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(has_args());
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsDetectLanguageFunction::Run() {
  std::optional<tabs::DetectLanguage::Params> params =
      tabs::DetectLanguage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction() = default;

ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() = default;

ExecuteCodeFunction::InitResult ExecuteCodeInTabFunction::Init() {
  NOTIMPLEMENTED();
  return set_init_result(VALIDATION_FAILURE);
}

bool ExecuteCodeInTabFunction::ShouldInsertCSS() const {
  return false;
}

bool ExecuteCodeInTabFunction::ShouldRemoveCSS() const {
  return false;
}

bool ExecuteCodeInTabFunction::CanExecuteScriptOnPage(std::string* error) {
  NOTIMPLEMENTED();
  return false;
}

ScriptExecutor* ExecuteCodeInTabFunction::GetScriptExecutor(
    std::string* error) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool ExecuteCodeInTabFunction::IsWebView() const {
  return false;
}

int ExecuteCodeInTabFunction::GetRootFrameId() const {
  NOTIMPLEMENTED();
  return ExtensionApiFrameIdMap::kTopFrameId;
}

const GURL& ExecuteCodeInTabFunction::GetWebViewSrc() const {
  NOTIMPLEMENTED();
  return GURL::EmptyGURL();
}

bool TabsInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

bool TabsRemoveCSSFunction::ShouldRemoveCSS() const {
  return true;
}

ExtensionFunction::ResponseAction TabsSetZoomFunction::Run() {
  std::optional<tabs::SetZoom::Params> params =
      tabs::SetZoom::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsGetZoomFunction::Run() {
  std::optional<tabs::GetZoom::Params> params =
      tabs::GetZoom::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsSetZoomSettingsFunction::Run() {
  std::optional<tabs::SetZoomSettings::Params> params =
      tabs::SetZoomSettings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsGetZoomSettingsFunction::Run() {
  std::optional<tabs::GetZoomSettings::Params> params =
      tabs::GetZoomSettings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

TabsDiscardFunction::TabsDiscardFunction() = default;

ExtensionFunction::ResponseAction TabsDiscardFunction::Run() {
  std::optional<tabs::Discard::Params> params =
      tabs::Discard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsGoForwardFunction::Run() {
  std::optional<tabs::GoForward::Params> params =
      tabs::GoForward::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsGoBackFunction::Run() {
  std::optional<tabs::GoBack::Params> params =
      tabs::GoBack::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

}  // namespace extensions
