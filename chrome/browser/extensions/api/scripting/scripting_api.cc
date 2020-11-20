// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/scripting/scripting_api.h"

#include <utility>

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/common/extensions/api/scripting.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/load_and_localize_file.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace {

constexpr char kCouldNotLoadFileError[] = "Could not load file: '*'.";

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

// Returns true if the `target` can be accessed with the given `permissions`.
// If the target can be accessed, populates `script_executor_out`,
// `frame_scope_out`, and `frame_ids_out` with the appropriate values;
// if the target cannot be accessed, populates `error_out`.
bool CanAccessTarget(const PermissionsData& permissions,
                     const api::scripting::InjectionTarget& target,
                     content::BrowserContext* browser_context,
                     bool include_incognito_information,
                     ScriptExecutor** script_executor_out,
                     ScriptExecutor::FrameScope* frame_scope_out,
                     std::vector<int>* frame_ids_out,
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

  if ((target.all_frames && *target.all_frames == true) && target.frame_ids) {
    *error_out = "Cannot specify both 'allFrames' and 'frameIds'.";
    return false;
  }

  ScriptExecutor* script_executor = tab_helper->script_executor();
  DCHECK(script_executor);

  ScriptExecutor::FrameScope frame_scope =
      target.all_frames && *target.all_frames == true
          ? ScriptExecutor::INCLUDE_SUB_FRAMES
          : ScriptExecutor::SPECIFIED_FRAMES;

  std::vector<int> frame_ids;
  if (target.frame_ids) {
    // NOTE: This creates a copy, but it's should always be very cheap, and it
    // lets us keep |target| const.
    frame_ids = *target.frame_ids;
  } else {
    frame_ids.push_back(ExtensionApiFrameIdMap::kTopFrameId);
  }

  // TODO(devlin): We error out if the extension doesn't have access to the top
  // frame, even if it may inject in child frames if allFrames is true. This is
  // inconsistent with content scripts (which can execute on child frames), but
  // consistent with the old tabs.executeScript() API.
  for (int frame_id : frame_ids) {
    content::RenderFrameHost* frame =
        ExtensionApiFrameIdMap::GetRenderFrameHostById(tab, frame_id);
    if (!frame) {
      *error_out = base::StringPrintf("No frame with id %d in tab with id %d",
                                      frame_id, target.tab_id);
      return false;
    }

    DCHECK_EQ(content::WebContents::FromRenderFrameHost(frame), tab);
    if (!HasPermissionToInjectIntoFrame(permissions, target.tab_id, frame,
                                        error_out)) {
      return false;
    }
  }

  *frame_ids_out = std::move(frame_ids);
  *frame_scope_out = frame_scope;
  *script_executor_out = script_executor;
  return true;
}

// Returns true if the loaded resource is valid for injection.
bool CheckLoadedResource(bool success,
                         std::string* data,
                         const std::string& file_name,
                         std::string* error) {
  if (!success) {
    *error = ErrorUtils::FormatErrorMessage(kCouldNotLoadFileError, file_name);
    return false;
  }

  DCHECK(data);
  // TODO(devlin): What necessitates this encoding requirement? Is it needed for
  // blink injection?
  if (!base::IsStringUTF8(*data)) {
    constexpr char kBadFileEncodingError[] =
        "Could not load file '*'. It isn't UTF-8 encoded.";
    *error = ErrorUtils::FormatErrorMessage(kBadFileEncodingError, file_name);
    return false;
  }

  return true;
}

// Checks the specified `files` for validity, and attempts to load and localize
// them, invoking `callback` with the result. Returns true on success; on
// failure, populates `error`.
bool CheckAndLoadFiles(const std::vector<std::string>& files,
                       const Extension& extension,
                       bool requires_localization,
                       LoadAndLocalizeResourceCallback callback,
                       std::string* error) {
  if (files.size() != 1) {
    constexpr char kExactlyOneFileError[] =
        "Exactly one file must be specified.";
    *error = kExactlyOneFileError;
    return false;
  }
  ExtensionResource resource = extension.GetResource(files[0]);
  if (resource.extension_root().empty() || resource.relative_path().empty()) {
    *error = ErrorUtils::FormatErrorMessage(kCouldNotLoadFileError, files[0]);
    return false;
  }

  LoadAndLocalizeResource(extension, resource, requires_localization,
                          std::move(callback));
  return true;
}

}  // namespace

ScriptingExecuteScriptFunction::ScriptingExecuteScriptFunction() = default;
ScriptingExecuteScriptFunction::~ScriptingExecuteScriptFunction() = default;

ExtensionFunction::ResponseAction ScriptingExecuteScriptFunction::Run() {
  std::unique_ptr<api::scripting::ExecuteScript::Params> params(
      api::scripting::ExecuteScript::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  injection_ = std::move(params->injection);

  if ((injection_.files && injection_.function) ||
      (!injection_.files && !injection_.function)) {
    return RespondNow(
        Error("Exactly one of 'function' and 'files' must be specified"));
  }

  if (injection_.files) {
    // JS files don't require localization.
    constexpr bool kRequiresLocalization = false;
    std::string error;
    if (!CheckAndLoadFiles(
            *injection_.files, *extension(), kRequiresLocalization,
            base::BindOnce(&ScriptingExecuteScriptFunction::DidLoadResource,
                           this),
            &error)) {
      return RespondNow(Error(std::move(error)));
    }
    return RespondLater();
  }

  DCHECK(injection_.function);

  // TODO(devlin): This (wrapping a function to create an IIFE) is pretty hacky,
  // and won't work well when we support currying arguments. Add support to the
  // ScriptExecutor to better support this case.
  std::string code_to_execute =
      base::StringPrintf("(%s)()", injection_.function->c_str());

  std::string error;
  if (!Execute(std::move(code_to_execute), /*script_src=*/GURL(), &error))
    return RespondNow(Error(std::move(error)));

  return RespondLater();
}

void ScriptingExecuteScriptFunction::DidLoadResource(
    bool success,
    std::unique_ptr<std::string> data) {
  DCHECK(injection_.files);
  DCHECK_EQ(1u, injection_.files->size());

  std::string error;
  if (!CheckLoadedResource(success, data.get(), injection_.files->at(0),
                           &error)) {
    Respond(Error(std::move(error)));
    return;
  }

  GURL script_url = extension()->GetResourceURL(injection_.files->at(0));
  if (!Execute(std::move(*data), std::move(script_url), &error))
    Respond(Error(std::move(error)));
}

bool ScriptingExecuteScriptFunction::Execute(std::string code_to_execute,
                                             GURL script_url,
                                             std::string* error) {
  ScriptExecutor* script_executor = nullptr;
  ScriptExecutor::FrameScope frame_scope = ScriptExecutor::SPECIFIED_FRAMES;
  std::vector<int> frame_ids;
  if (!CanAccessTarget(*extension()->permissions_data(), injection_.target,
                       browser_context(), include_incognito_information(),
                       &script_executor, &frame_scope, &frame_ids, error)) {
    return false;
  }

  script_executor->ExecuteScript(
      HostID(HostID::EXTENSIONS, extension()->id()), UserScript::ADD_JAVASCRIPT,
      std::move(code_to_execute), frame_scope, frame_ids,
      ScriptExecutor::MATCH_ABOUT_BLANK, UserScript::DOCUMENT_IDLE,
      ScriptExecutor::DEFAULT_PROCESS,
      /* webview_src */ GURL(), std::move(script_url), user_gesture(),
      base::nullopt, ScriptExecutor::JSON_SERIALIZED_RESULT,
      base::BindOnce(&ScriptingExecuteScriptFunction::OnScriptExecuted, this));

  return true;
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

  injection_ = std::move(params->injection);

  if ((injection_.files && injection_.css) ||
      (!injection_.files && !injection_.css)) {
    return RespondNow(
        Error("Exactly one of 'css' and 'files' must be specified"));
  }

  if (injection_.files) {
    // CSS files require localization.
    constexpr bool kRequiresLocalization = true;
    std::string error;
    if (!CheckAndLoadFiles(
            *injection_.files, *extension(), kRequiresLocalization,
            base::BindOnce(&ScriptingInsertCSSFunction::DidLoadResource, this),
            &error)) {
      return RespondNow(Error(std::move(error)));
    }
    return RespondLater();
  }

  DCHECK(injection_.css);

  std::string error;
  if (!Execute(std::move(*injection_.css), /*script_url=*/GURL(), &error)) {
    return RespondNow(Error(std::move(error)));
  }

  return RespondLater();
}

void ScriptingInsertCSSFunction::DidLoadResource(
    bool success,
    std::unique_ptr<std::string> data) {
  DCHECK(injection_.files);
  DCHECK_EQ(1u, injection_.files->size());

  std::string error;
  if (!CheckLoadedResource(success, data.get(), injection_.files->at(0),
                           &error)) {
    Respond(Error(std::move(error)));
    return;
  }

  GURL script_url = extension()->GetResourceURL(injection_.files->at(0));
  if (!Execute(std::move(*data), std::move(script_url), &error))
    Respond(Error(std::move(error)));
}

bool ScriptingInsertCSSFunction::Execute(std::string code_to_execute,
                                         GURL script_url,
                                         std::string* error) {
  ScriptExecutor* script_executor = nullptr;
  ScriptExecutor::FrameScope frame_scope = ScriptExecutor::SPECIFIED_FRAMES;
  std::vector<int> frame_ids;
  if (!CanAccessTarget(*extension()->permissions_data(), injection_.target,
                       browser_context(), include_incognito_information(),
                       &script_executor, &frame_scope, &frame_ids, error)) {
    return false;
  }
  DCHECK(script_executor);

  // TODO(devlin): Pull the default argument for CSSOrigin up to here, and
  // pass it through ScriptExecutor and friends, rather than having it
  // defined in the renderer.
  base::Optional<CSSOrigin> origin;
  switch (injection_.origin) {
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
      std::move(code_to_execute), frame_scope, frame_ids,
      ScriptExecutor::MATCH_ABOUT_BLANK, kRunLocation,
      ScriptExecutor::DEFAULT_PROCESS,
      /* webview_src */ GURL(), std::move(script_url), user_gesture(), origin,
      ScriptExecutor::NO_RESULT,
      base::BindOnce(&ScriptingInsertCSSFunction::OnCSSInserted, this));

  return true;
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
