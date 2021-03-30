// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/scripting/scripting_api.h"

#include <algorithm>
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
#include "extensions/common/mojom/action_type.mojom-shared.h"
#include "extensions/common/mojom/css_origin.mojom-shared.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace {

constexpr char kCouldNotLoadFileError[] = "Could not load file: '*'.";
constexpr char kExactlyOneOfCssAndFilesError[] =
    "Exactly one of 'css' and 'files' must be specified.";

// Note: CSS always injects as soon as possible, so we default to
// document_start. Because of tab loading, there's no guarantee this will
// *actually* inject before page load, but it will at least inject "soon".
constexpr mojom::RunLocation kCSSRunLocation =
    mojom::RunLocation::kDocumentStart;

// Converts the given `style_origin` to a CSSOrigin.
mojom::CSSOrigin ConvertStyleOriginToCSSOrigin(
    api::scripting::StyleOrigin style_origin) {
  mojom::CSSOrigin css_origin = mojom::CSSOrigin::kAuthor;
  switch (style_origin) {
    case api::scripting::STYLE_ORIGIN_NONE:
    case api::scripting::STYLE_ORIGIN_AUTHOR:
      css_origin = mojom::CSSOrigin::kAuthor;
      break;
    case api::scripting::STYLE_ORIGIN_USER:
      css_origin = mojom::CSSOrigin::kUser;
      break;
  }

  return css_origin;
}

// Checks `files` and populates `resource_out` with the appropriate extension
// resource. Returns true on success; on failure, populates `error_out`.
bool GetFileResource(const std::vector<std::string>& files,
                     const Extension& extension,
                     ExtensionResource* resource_out,
                     std::string* error_out) {
  if (files.size() != 1) {
    constexpr char kExactlyOneFileError[] =
        "Exactly one file must be specified.";
    *error_out = kExactlyOneFileError;
    return false;
  }
  ExtensionResource resource = extension.GetResource(files[0]);
  if (resource.extension_root().empty() || resource.relative_path().empty()) {
    *error_out =
        ErrorUtils::FormatErrorMessage(kCouldNotLoadFileError, files[0]);
    return false;
  }

  *resource_out = std::move(resource);
  return true;
}

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
      if (permissions.HasAPIPermission(mojom::APIPermissionID::kTab)) {
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
                     std::set<int>* frame_ids_out,
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

  std::set<int> frame_ids;
  if (target.frame_ids) {
    frame_ids.insert(target.frame_ids->begin(), target.frame_ids->end());
  } else {
    frame_ids.insert(ExtensionApiFrameIdMap::kTopFrameId);
  }

  // TODO(devlin): If `allFrames` is true, we error out if the extension
  // doesn't have access to the top frame (even if it may inject in child
  // frames). This is inconsistent with content scripts (which can execute on
  // child frames), but consistent with the old tabs.executeScript() API.
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
  ExtensionResource resource;
  if (!GetFileResource(files, extension, &resource, error))
    return false;

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
  std::set<int> frame_ids;
  if (!CanAccessTarget(*extension()->permissions_data(), injection_.target,
                       browser_context(), include_incognito_information(),
                       &script_executor, &frame_scope, &frame_ids, error)) {
    return false;
  }

  script_executor->ExecuteScript(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension()->id()),
      mojom::ActionType::kAddJavascript, std::move(code_to_execute),
      frame_scope, frame_ids, ScriptExecutor::MATCH_ABOUT_BLANK,
      mojom::RunLocation::kDocumentIdle, ScriptExecutor::DEFAULT_PROCESS,
      /* webview_src */ GURL(), std::move(script_url), user_gesture(),
      mojom::CSSOrigin::kAuthor, ScriptExecutor::JSON_SERIALIZED_RESULT,
      base::BindOnce(&ScriptingExecuteScriptFunction::OnScriptExecuted, this));

  return true;
}

void ScriptingExecuteScriptFunction::OnScriptExecuted(
    std::vector<ScriptExecutor::FrameResult> frame_results) {
  // If only a single frame was included and the injection failed, respond with
  // an error.
  if (frame_results.size() == 1 && !frame_results[0].error.empty()) {
    Respond(Error(std::move(frame_results[0].error)));
    return;
  }

  // Otherwise, respond successfully. We currently just skip over individual
  // frames that failed. In the future, we can bubble up these error messages
  // to the extension.
  std::vector<api::scripting::InjectionResult> injection_results;
  for (auto& result : frame_results) {
    if (!result.error.empty())
      continue;
    api::scripting::InjectionResult injection_result;
    injection_result.result =
        base::Value::ToUniquePtrValue(std::move(result.value));
    injection_result.frame_id = result.frame_id;

    // Put the top frame first; otherwise, any order.
    if (result.frame_id == ExtensionApiFrameIdMap::kTopFrameId) {
      injection_results.insert(injection_results.begin(),
                               std::move(injection_result));
    } else {
      injection_results.push_back(std::move(injection_result));
    }
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
    return RespondNow(Error(kExactlyOneOfCssAndFilesError));
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
  std::set<int> frame_ids;
  if (!CanAccessTarget(*extension()->permissions_data(), injection_.target,
                       browser_context(), include_incognito_information(),
                       &script_executor, &frame_scope, &frame_ids, error)) {
    return false;
  }
  DCHECK(script_executor);

  script_executor->ExecuteScript(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension()->id()),
      mojom::ActionType::kAddCss, std::move(code_to_execute), frame_scope,
      frame_ids, ScriptExecutor::MATCH_ABOUT_BLANK, kCSSRunLocation,
      ScriptExecutor::DEFAULT_PROCESS,
      /* webview_src */ GURL(), std::move(script_url), user_gesture(),
      ConvertStyleOriginToCSSOrigin(injection_.origin),
      ScriptExecutor::NO_RESULT,
      base::BindOnce(&ScriptingInsertCSSFunction::OnCSSInserted, this));

  return true;
}

void ScriptingInsertCSSFunction::OnCSSInserted(
    std::vector<ScriptExecutor::FrameResult> results) {
  // If only a single frame was included and the injection failed, respond with
  // an error.
  if (results.size() == 1 && !results[0].error.empty()) {
    Respond(Error(std::move(results[0].error)));
    return;
  }

  Respond(NoArguments());
}

ScriptingRemoveCSSFunction::ScriptingRemoveCSSFunction() = default;
ScriptingRemoveCSSFunction::~ScriptingRemoveCSSFunction() = default;

ExtensionFunction::ResponseAction ScriptingRemoveCSSFunction::Run() {
  std::unique_ptr<api::scripting::RemoveCSS::Params> params(
      api::scripting::RemoveCSS::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  api::scripting::CSSInjection& injection = params->injection;

  if ((injection.files && injection.css) ||
      (!injection.files && !injection.css)) {
    return RespondNow(Error(kExactlyOneOfCssAndFilesError));
  }

  GURL script_url;
  std::string error;
  std::string code;
  if (injection.files) {
    // Note: Since we're just removing the CSS, we don't actually need to load
    // the file here. It's okay for `code` to be empty in this case.
    ExtensionResource resource;
    if (!GetFileResource(*injection.files, *extension(), &resource, &error))
      return RespondNow(Error(std::move(error)));

    script_url = extension()->GetResourceURL(injection.files->at(0));
  } else {
    DCHECK(injection.css);
    code = std::move(*injection.css);
  }

  ScriptExecutor* script_executor = nullptr;
  ScriptExecutor::FrameScope frame_scope = ScriptExecutor::SPECIFIED_FRAMES;
  std::set<int> frame_ids;
  if (!CanAccessTarget(*extension()->permissions_data(), injection.target,
                       browser_context(), include_incognito_information(),
                       &script_executor, &frame_scope, &frame_ids, &error)) {
    return RespondNow(Error(std::move(error)));
  }
  DCHECK(script_executor);

  DCHECK(code.empty() || !script_url.is_valid());

  script_executor->ExecuteScript(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension()->id()),
      mojom::ActionType::kRemoveCss, std::move(code), frame_scope, frame_ids,
      ScriptExecutor::MATCH_ABOUT_BLANK, kCSSRunLocation,
      ScriptExecutor::DEFAULT_PROCESS,
      /* webview_src */ GURL(), std::move(script_url), user_gesture(),
      ConvertStyleOriginToCSSOrigin(injection.origin),
      ScriptExecutor::NO_RESULT,
      base::BindOnce(&ScriptingRemoveCSSFunction::OnCSSRemoved, this));

  return RespondLater();
}

void ScriptingRemoveCSSFunction::OnCSSRemoved(
    std::vector<ScriptExecutor::FrameResult> results) {
  // If only a single frame was included and the injection failed, respond with
  // an error.
  if (results.size() == 1 && !results[0].error.empty()) {
    Respond(Error(std::move(results[0].error)));
    return;
  }

  Respond(NoArguments());
}

}  // namespace extensions
