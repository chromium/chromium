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
                   ScriptExecutor::FrameScope frame_scope,
                   bool user_gesture,
                   ScriptExecutor::ScriptFinishedCallback callback) {
  script_executor->ExecuteScript(
      HostID(HostID::EXTENSIONS, extension.id()), UserScript::ADD_JAVASCRIPT,
      code, frame_scope, {ExtensionApiFrameIdMap::kTopFrameId},
      ScriptExecutor::MATCH_ABOUT_BLANK, UserScript::DOCUMENT_IDLE,
      ScriptExecutor::DEFAULT_PROCESS,
      /* webview_src */ GURL(), /* script_url */ GURL(), user_gesture,
      base::nullopt, ScriptExecutor::JSON_SERIALIZED_RESULT,
      std::move(callback));
}

// Returns true if the `target` can be accessed with the given `permissions`.
// If the target can be accessed, populates `script_executor_out` with the
// associated ScriptExecutor and `frame_scope_out` with the appropriate scope;
// if the target cannot be accessed, populates `error_out`.
bool CanAccessTarget(const PermissionsData& permissions,
                     const api::scripting::InjectionTarget& target,
                     content::BrowserContext* browser_context,
                     bool include_incognito_information,
                     ScriptExecutor** script_executor_out,
                     ScriptExecutor::FrameScope* frame_scope_out,
                     std::string* error_out) {
  content::WebContents* tab = nullptr;
  TabHelper* tab_helper = nullptr;
  if (!ExtensionTabUtil::GetTabById(target.tab_id, browser_context,
                                    include_incognito_information, &tab) ||
      !(tab_helper = TabHelper::FromWebContents(tab))) {
    // TODO(devlin): Add a constant for this in a centrally-consumable location.
    *error_out = base::StringPrintf("No tab with id: %d", target.tab_id);
    return false;
  }

  ScriptExecutor* script_executor = tab_helper->script_executor();
  DCHECK(script_executor);

  ScriptExecutor::FrameScope frame_scope =
      target.all_frames && *target.all_frames == true
          ? ScriptExecutor::INCLUDE_SUB_FRAMES
          : ScriptExecutor::SPECIFIED_FRAMES;

  // TODO(devlin): It'd be best to do all the permission checks for the frames
  // on the browser side, including child frames. Today, we only check the
  // main frame, and then let the ScriptExecutor inject into all child frames
  // (there's a permission check at the time of the injection in the renderer).
  // TODO(devlin): We error out if the extension doesn't have access to the top
  // frame, even if it may inject in child frames. This is inconsistent with
  // content scripts (which can execute on child frames), but consistent with
  // the old tabs.executeScript() API.
  if (!HasPermissionToInject(permissions, target.tab_id, tab, error_out))
    return false;

  *frame_scope_out = frame_scope;
  *script_executor_out = script_executor;
  return true;
}

}  // namespace

ScriptingExecuteScriptFunction::ScriptingExecuteScriptFunction() = default;
ScriptingExecuteScriptFunction::~ScriptingExecuteScriptFunction() = default;

ExtensionFunction::ResponseAction ScriptingExecuteScriptFunction::Run() {
  std::unique_ptr<api::scripting::ExecuteScript::Params> params(
      api::scripting::ExecuteScript::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  const api::scripting::ScriptInjection& injection = params->injection;

  // TODO(devlin): Add support for specifying a file.
  if (!injection.function)
    return RespondNow(Error("'css' must be specified"));

  std::string error;
  ScriptExecutor* script_executor = nullptr;
  ScriptExecutor::FrameScope frame_scope = ScriptExecutor::SPECIFIED_FRAMES;
  if (!CanAccessTarget(*extension()->permissions_data(), injection.target,
                       browser_context(), include_incognito_information(),
                       &script_executor, &frame_scope, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  DCHECK(script_executor);

  // TODO(devlin): This (wrapping a function to create an IIFE) is pretty hacky,
  // and won't work well when we support currying arguments. Add support to the
  // ScriptExecutor to better support this case.
  std::string code_to_execute =
      base::StringPrintf("(%s)()", injection.function->c_str());

  ExecuteScript(
      script_executor, code_to_execute, *extension(), frame_scope,
      user_gesture(),
      base::BindOnce(&ScriptingExecuteScriptFunction::OnScriptExecuted, this));

  return RespondLater();
}

void ScriptingExecuteScriptFunction::OnScriptExecuted(
    const std::string& error,
    const GURL& frame_url,
    const base::ListValue& result) {
  if (!error.empty()) {
    Respond(Error(error));
    return;
  }

  std::vector<api::scripting::InjectionResult> injection_results;

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

ScriptingInsertCSSFunction::ScriptingInsertCSSFunction() = default;
ScriptingInsertCSSFunction::~ScriptingInsertCSSFunction() = default;

ExtensionFunction::ResponseAction ScriptingInsertCSSFunction::Run() {
  std::unique_ptr<api::scripting::InsertCSS::Params> params(
      api::scripting::InsertCSS::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  api::scripting::CSSInjection injection = std::move(params->injection);

  // TODO(https://crbug.com/1148880): Add support for specifying a file.
  if (!injection.css)
    return RespondNow(Error("'css' must be specified"));

  std::string error;
  ScriptExecutor* script_executor = nullptr;
  ScriptExecutor::FrameScope frame_scope = ScriptExecutor::SPECIFIED_FRAMES;
  if (!CanAccessTarget(*extension()->permissions_data(), injection.target,
                       browser_context(), include_incognito_information(),
                       &script_executor, &frame_scope, &error)) {
    return RespondNow(Error(std::move(error)));
  }
  DCHECK(script_executor);

  // TODO(devlin): Pull the default argument for CSSOrigin up to here, and
  // pass it through ScriptExecutor and friends, rather than having it
  // defined in the renderer.
  base::Optional<CSSOrigin> origin;
  switch (injection.origin) {
    case api::scripting::STYLE_ORIGIN_USER:
      origin = CSS_ORIGIN_USER;
      break;
    case api::scripting::STYLE_ORIGIN_AUTHOR:
      origin = CSS_ORIGIN_AUTHOR;
      break;
    case api::scripting::STYLE_ORIGIN_NONE:
      break;
  }

  // Note: CSS always injects as soon as possible, so we default to
  // document_start. Because of tab loading, there's no guarantee this will
  // *actually* inject before page load, but it will at least inject "soon".
  constexpr UserScript::RunLocation kRunLocation = UserScript::DOCUMENT_START;
  script_executor->ExecuteScript(
      HostID(HostID::EXTENSIONS, extension()->id()), UserScript::ADD_CSS,
      *injection.css, frame_scope, {ExtensionApiFrameIdMap::kTopFrameId},
      ScriptExecutor::MATCH_ABOUT_BLANK, kRunLocation,
      ScriptExecutor::DEFAULT_PROCESS,
      /* webview_src */ GURL(), /* script_url */ GURL(), user_gesture(), origin,
      ScriptExecutor::NO_RESULT,
      base::BindOnce(&ScriptingInsertCSSFunction::OnCSSInserted, this));

  return RespondLater();
}

void ScriptingInsertCSSFunction::OnCSSInserted(const std::string& error,
                                               const GURL& frame_url,
                                               const base::ListValue& result) {
  DCHECK(result.GetList().empty());
  if (!error.empty()) {
    Respond(Error(error));
    return;
  }
  Respond(NoArguments());
}

}  // namespace extensions
