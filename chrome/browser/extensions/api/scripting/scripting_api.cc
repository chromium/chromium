// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/scripting/scripting_api.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/common/extensions/api/scripting.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace {

// Returns true if the `permissions` allow for injection into the given `frame`.
// If false, populates `error`.
bool HasPermissionToInjectIntoFrame(const PermissionsData& permissions,
                                    int tab_id,
                                    content::RenderFrameHost* frame,
                                    std::string* error) {
  GURL url = frame->GetLastCommittedURL();

  // TODO(devlin): Add more schemes here, in line with
  // https://crbug.com/55084.
  if (url.SchemeIs(url::kAboutScheme) || url.SchemeIs(url::kDataScheme)) {
    url::Origin origin = frame->GetLastCommittedOrigin();
    const url::SchemeHostPort& tuple_or_precursor_tuple =
        origin.GetTupleOrPrecursorTupleIfOpaque();
    if (!tuple_or_precursor_tuple.IsValid()) {
      if (permissions.HasAPIPermission(APIPermission::kTab)) {
        *error = ErrorUtils::FormatErrorMessage(
            manifest_errors::kCannotAccessPageWithUrl, url.spec());
      } else {
        *error = manifest_errors::kCannotAccessPage;
      }
      return false;
    }

    url = tuple_or_precursor_tuple.GetURL();
  }

  return permissions.CanAccessPage(url, tab_id, error);
}

// Returns true if the `permissions` allow for injection into the given `tab`.
// If false, populates `error`.
bool HasPermissionToInject(const PermissionsData& permissions,
                           int tab_id,
                           content::WebContents* tab,
                           std::string* error) {
  // TODO(devlin): Support specifying multiple frames.
  return HasPermissionToInjectIntoFrame(permissions, tab_id,
                                        tab->GetMainFrame(), error);
}

void ExecuteScript(ScriptExecutor* script_executor,
                   const std::string& code,
                   const Extension& extension,
                   bool user_gesture,
                   ScriptExecutor::ScriptFinishedCallback callback) {
  script_executor->ExecuteScript(
      HostID(HostID::EXTENSIONS, extension.id()), UserScript::ADD_JAVASCRIPT,
      code, ScriptExecutor::SINGLE_FRAME, ExtensionApiFrameIdMap::kTopFrameId,
      ScriptExecutor::MATCH_ABOUT_BLANK, UserScript::DOCUMENT_IDLE,
      ScriptExecutor::DEFAULT_PROCESS,
      /* webview_src */ GURL(), /* script_url */ GURL(), user_gesture,
      base::nullopt, ScriptExecutor::JSON_SERIALIZED_RESULT,
      std::move(callback));
}

}  // namespace

ScriptingExecuteScriptFunction::ScriptingExecuteScriptFunction() = default;
ScriptingExecuteScriptFunction::~ScriptingExecuteScriptFunction() = default;

ExtensionFunction::ResponseAction ScriptingExecuteScriptFunction::Run() {
  std::unique_ptr<api::scripting::ExecuteScript::Params> params(
      api::scripting::ExecuteScript::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  const api::scripting::ScriptInjection& injection = params->injection;

  content::WebContents* tab = nullptr;
  TabHelper* tab_helper = nullptr;
  if (!ExtensionTabUtil::GetTabById(
          injection.target.tab_id, browser_context(),
          include_incognito_information(), nullptr /* browser */,
          nullptr /* tab_strip */, &tab, nullptr /* tab_index */) ||
      !(tab_helper = TabHelper::FromWebContents(tab))) {
    return RespondNow(Error(
        base::StringPrintf("No tab with id: %d", injection.target.tab_id)));
  }

  ScriptExecutor* script_executor = tab_helper->script_executor();
  DCHECK(script_executor);

  std::string error;
  if (!HasPermissionToInject(*extension()->permissions_data(),
                             injection.target.tab_id, tab, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  EXTENSION_FUNCTION_VALIDATE(injection.function);

  // TODO(devlin): This (wrapping a function to create an IIFE) is pretty hacky,
  // and won't work well when we support currying arguments. Add support to the
  // ScriptExecutor to better support this case.
  std::string code_to_execute =
      base::StringPrintf("(%s)()", injection.function->c_str());

  ExecuteScript(
      script_executor, code_to_execute, *extension(), user_gesture(),
      base::BindOnce(&ScriptingExecuteScriptFunction::OnScriptExecuted, this));

  return RespondLater();
}

void ScriptingExecuteScriptFunction::OnScriptExecuted(
    const std::string& error,
    const GURL& frame_url,
    const base::ListValue& result) {
  std::vector<api::scripting::InjectionResult> injection_results;

  // TODO(devlin): Remove this check when we support multiple frame injection.
  DCHECK_EQ(1u, result.GetList().size());

  // TODO(devlin): This results in a few copies of values. It'd be better if our
  // auto-generated code supported moved-in parameters for result construction.
  for (const auto& value : result.GetList()) {
    api::scripting::InjectionResult injection_result;
    injection_result.result = std::make_unique<base::Value>(value.Clone());
    injection_results.push_back(std::move(injection_result));
  }

  Respond(ArgumentList(
      api::scripting::ExecuteScript::Results::Create(injection_results)));
}

}  // namespace extensions
