// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webrtc_logging_private/webrtc_logging_private_api.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/error_utils.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "extensions/common/permissions/permissions_data.h"
#endif

namespace {

bool CanEnableAudioDebugRecordingsFromExtension(
    const extensions::Extension* extension) {
  bool enabled_by_permissions = false;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (extension) {
    enabled_by_permissions =
        extension->permissions_data()->active_permissions().HasAPIPermission(
            extensions::mojom::APIPermissionID::
                kWebrtcLoggingPrivateAudioDebug);
  }
#endif
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             ::switches::kEnableAudioDebugRecordingsFromExtension) ||
         enabled_by_permissions;
}

}  // namespace

namespace extensions {

using api::webrtc_logging_private::MetaDataEntry;
using content::BrowserThread;

namespace Discard = api::webrtc_logging_private::Discard;
namespace SetMetaData = api::webrtc_logging_private::SetMetaData;
namespace SetUploadOnRenderClose =
    api::webrtc_logging_private::SetUploadOnRenderClose;
namespace Start = api::webrtc_logging_private::Start;
namespace StartRtpDump = api::webrtc_logging_private::StartRtpDump;
namespace Stop = api::webrtc_logging_private::Stop;
namespace StopRtpDump = api::webrtc_logging_private::StopRtpDump;
namespace Store = api::webrtc_logging_private::Store;
namespace Upload = api::webrtc_logging_private::Upload;
namespace UploadStored = api::webrtc_logging_private::UploadStored;
namespace StartAudioDebugRecordings =
    api::webrtc_logging_private::StartAudioDebugRecordings;
namespace StopAudioDebugRecordings =
    api::webrtc_logging_private::StopAudioDebugRecordings;
namespace StartEventLogging = api::webrtc_logging_private::StartEventLogging;
namespace GetLogsDirectory = api::webrtc_logging_private::GetLogsDirectory;

namespace {
std::string HashIdWithOrigin(const std::string& security_origin,
                             const std::string& log_id) {
  return base::NumberToString(base::Hash(security_origin + log_id));
}
}  // namespace

// TODO(hlundin): Consolidate with WebrtcAudioPrivateFunction and improve.
// http://crbug.com/710371
content::RenderProcessHost* WebrtcLoggingPrivateFunction::RphFromRequest(
    const api::webrtc_logging_private::RequestInfo& request,
    const std::string& security_origin,
    std::string* error) {
  // There are 2 ways these API functions can get called.
  //
  //  1. From an allowlisted component extension on behalf of a page with the
  //  appropriate origin via a message from that page. In this case, either the
  //  guest process id or the tab id is on the message received by the component
  //  extension, and the extension can pass that along in RequestInfo as
  //  |guest_process_id| or |tab_id|.
  //
  //  2. From an allowlisted app that hosts a page in a webview. In this case,
  //  the app should call these API functions with the |target_webview| flag
  //  set, from a web contents that has exactly 1 webview .

  // If |target_webview| is set, lookup the guest content's render process in
  // the sender's web contents. There should be exactly 1 guest.
  if (request.target_webview && *request.target_webview) {
    content::RenderProcessHost* target_host = nullptr;
    int guests_found = 0;
    auto* guest_view_manager =
        guest_view::GuestViewManager::FromBrowserContext(browser_context());
    if (!guest_view_manager) {
      // Called from a context without guest views. Bail with an appropriate
      // error.
      *error = "No guest view manager found";
      return nullptr;
    }
    guest_view_manager->ForEachGuest(
        GetSenderWebContents(), [&](content::WebContents* guest_contents) {
          ++guests_found;
          target_host = guest_contents->GetPrimaryMainFrame()->GetProcess();
          // Don't short-circuit, so we can count how many other guest contents
          // there are.
          return false;
        });
    if (!target_host) {
      *error = "No webview render process found";
      return nullptr;
    }
    if (guests_found > 1) {
      *error = "Multiple webviews found";
      return nullptr;
    }
    return target_host;
  }

  // If |guest_process_id| is defined, directly use this id to find the
  // corresponding RenderProcessHost.
  if (request.guest_process_id) {
    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(*request.guest_process_id);
    if (!rph) {
      *error =
          base::StringPrintf("Failed to get RPH fro guest proccess ID (%d).",
                             *request.guest_process_id);
    }
    return rph;
  }

  // Otherwise, use the |tab_id|. If there's no |target_viewview|, no |tab_id|,
  // and no |guest_process_id|, we can't look up the RenderProcessHost.
  if (!request.tab_id) {
    *error = "No webview, tab ID, or guest process ID specified.";
    return nullptr;
  }

  int tab_id = *request.tab_id;
  content::WebContents* contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(tab_id, browser_context(), true,
                                    &contents)) {
    *error = extensions::ErrorUtils::FormatErrorMessage(
        extensions::ExtensionTabUtil::kTabNotFoundError,
        base::NumberToString(tab_id));
    return nullptr;
  }
  if (!contents) {
    *error = "Web contents for tab not found.";
    return nullptr;
  }
  GURL expected_origin =
      contents->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  if (expected_origin.spec() != security_origin) {
    *error = base::StringPrintf(
        "Invalid security origin. Expected=%s, actual=%s",
        expected_origin.spec().c_str(), security_origin.c_str());
    return nullptr;
  }

  content::RenderProcessHost* rph =
      contents->GetPrimaryMainFrame()->GetProcess();
  if (!rph) {
    *error = "Failed to get RPH.";
  }
  return rph;
}

WebRtcLoggingController*
WebrtcLoggingPrivateFunction::LoggingControllerFromRequest(
    const api::webrtc_logging_private::RequestInfo& request,
    const std::string& security_origin,
    std::string* error) {
  content::RenderProcessHost* host =
      RphFromRequest(request, security_origin, error);
  if (!host) {
    DCHECK(!error->empty()) << "|error| must be set by RphFromRequest()";
    return nullptr;
  }
  return WebRtcLoggingController::FromRenderProcessHost(host);
}

WebRtcLoggingController*
WebrtcLoggingPrivateFunctionWithGenericCallback::PrepareTask(
    const api::webrtc_logging_private::RequestInfo& request,
    const std::string& security_origin,
    WebRtcLoggingController::GenericDoneCallback* callback,
    std::string* error) {
  *callback = base::BindOnce(
      &WebrtcLoggingPrivateFunctionWithGenericCallback::FireCallback, this);
  return LoggingControllerFromRequest(request, security_origin, error);
}

void WebrtcLoggingPrivateFunctionWithGenericCallback::FireCallback(
    bool success, const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(error_message));
  }
}

void WebrtcLoggingPrivateFunctionWithUploadCallback::FireCallback(
    bool success, const std::string& report_id,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (success) {
    api::webrtc_logging_private::UploadResult result;
    result.report_id = report_id;
    Respond(WithArguments(result.ToValue()));
  } else {
    Respond(Error(error_message));
  }
}

void WebrtcLoggingPrivateFunctionWithRecordingDoneCallback::FireErrorCallback(
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Respond(Error(error_message));
}

void WebrtcLoggingPrivateFunctionWithRecordingDoneCallback::FireCallback(
    const std::string& prefix_path,
    bool did_stop,
    bool did_manual_stop) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  api::webrtc_logging_private::RecordingInfo result;
  result.prefix_path = prefix_path;
  result.did_stop = did_stop;
  result.did_manual_stop = did_manual_stop;
  Respond(WithArguments(result.ToValue()));
}

ExtensionFunction::ResponseAction
WebrtcLoggingPrivateSetMetaDataFunction::Run() {
  std::optional<SetMetaData::Params> params =
      SetMetaData::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WebRtcLoggingController::GenericDoneCallback callback;
  std::string error;
  WebRtcLoggingController* webrtc_logging_controller =
      PrepareTask(params->request, params->security_origin, &callback, &error);
  if (!webrtc_logging_controller)
    return RespondNow(Error(std::move(error)));

  std::unique_ptr<WebRtcLogMetaDataMap> meta_data(new WebRtcLogMetaDataMap());
  for (const MetaDataEntry& entry : params->meta_data)
    (*meta_data)[entry.key] = entry.value;

  webrtc_logging_controller->SetMetaData(std::move(meta_data),
                                         std::move(callback));
  return RespondLater();
}

ExtensionFunction::ResponseAction WebrtcLoggingPrivateStartFunction::Run() {
  std::optional<Start::Params> params = Start::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WebRtcLoggingController::GenericDoneCallback callback;
  std::string error;
  WebRtcLoggingController* webrtc_logging_controller =
      PrepareTask(params->request, params->security_origin, &callback, &error);
  if (!webrtc_logging_controller)
    return RespondNow(Error(std::move(error)));

  webrtc_logging_controller->StartLogging(std::move(callback));
  return RespondLater();
}

ExtensionFunction::ResponseAction
WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::Run() {
  std::optional<SetUploadOnRenderClose::Params> params =
      SetUploadOnRenderClose::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  WebRtcLoggingController* webrtc_logging_controller(
      LoggingControllerFromRequest(params->request, params->security_origin,
                                   &error));
  if (!webrtc_logging_controller)
    return RespondNow(Error(std::move(error)));

  webrtc_logging_controller->set_upload_log_on_render_close(
      params->should_upload);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction WebrtcLoggingPrivateStopFunction::Run() {
  std::optional<Stop::Params> params = Stop::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WebRtcLoggingController::GenericDoneCallback callback;
  std::string error;
  WebRtcLoggingController* webrtc_logging_controller =
      PrepareTask(params->request, params->security_origin, &callback, &error);
  if (!webrtc_logging_controller)
    return RespondNow(Error(std::move(error)));

  webrtc_logging_controller->StopLogging(std::move(callback));
  return RespondLater();
}

ExtensionFunction::ResponseAction WebrtcLoggingPrivateStoreFunction::Run() {
  std::optional<Store::Params> params = Store::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WebRtcLoggingController::GenericDoneCallback callback;
  std::string error;
  WebRtcLoggingController* webrtc_logging_controller =
      PrepareTask(params->request, params->security_origin, &callback, &error);
  if (!webrtc_logging_controller)
    return RespondNow(Error(std::move(error)));

  const std::string local_log_id(HashIdWithOrigin(params->security_origin,
                                                  params->log_id));

  webrtc_logging_controller->StoreLog(local_log_id, std::move(callback));
  return RespondLater();
}

ExtensionFunction::ResponseAction
WebrtcLoggingPrivateUploadStoredFunction::Run() {
  std::optional<UploadStored::Params> params =
      UploadStored::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  WebRtcLoggingController* logging_controller = LoggingControllerFromRequest(
      params->request, params->security_origin, &error);
  if (!logging_controller)
    return RespondNow(Error(std::move(error)));

  WebRtcLoggingController::UploadDoneCallback callback = base::BindOnce(
      &WebrtcLoggingPrivateUploadStoredFunction::FireCallback, this);

  const std::string local_log_id(HashIdWithOrigin(params->security_origin,
                                                  params->log_id));

  logging_controller->UploadStoredLog(local_log_id, std::move(callback));
  return RespondLater();
}

ExtensionFunction::ResponseAction WebrtcLoggingPrivateUploadFunction::Run() {
  std::optional<Upload::Params> params = Upload::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  WebRtcLoggingController* logging_controller = LoggingControllerFromRequest(
      params->request, params->security_origin, &error);
  if (!logging_controller)
    return RespondNow(Error(std::move(error)));

  WebRtcLoggingController::UploadDoneCallback callback =
      base::BindOnce(&WebrtcLoggingPrivateUploadFunction::FireCallback, this);

  logging_controller->UploadLog(std::move(callback));
  return RespondLater();
}

ExtensionFunction::ResponseAction WebrtcLoggingPrivateDiscardFunction::Run() {
  std::optional<Discard::Params> params = Discard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WebRtcLoggingController::GenericDoneCallback callback;
  std::string error;
  WebRtcLoggingController* webrtc_logging_controller =
      PrepareTask(params->request, params->security_origin, &callback, &error);
  if (!webrtc_logging_controller)
    return RespondNow(Error(std::move(error)));

  webrtc_logging_controller->DiscardLog(std::move(callback));
  return RespondLater();
}

ExtensionFunction::ResponseAction
WebrtcLoggingPrivateStartRtpDumpFunction::Run() {
  std::optional<StartRtpDump::Params> params =
      StartRtpDump::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->incoming && !params->outgoing) {
    FireCallback(false, "Either incoming or outgoing must be true.");
    return AlreadyResponded();
  }

  RtpDumpType type =
      (params->incoming && params->outgoing)
          ? RTP_DUMP_BOTH
          : (params->incoming ? RTP_DUMP_INCOMING : RTP_DUMP_OUTGOING);

  std::string error;
  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin, &error);
  if (!host) {
    return RespondNow(Error(std::move(error)));
  }

  WebRtcLoggingController* webrtc_logging_controller =
      WebRtcLoggingController::FromRenderProcessHost(host);

  WebRtcLoggingController::GenericDoneCallback callback = base::BindOnce(
      &WebrtcLoggingPrivateStartRtpDumpFunction::FireCallback, this);

  webrtc_logging_controller->StartRtpDump(type, std::move(callback));
  return RespondLater();
}

ExtensionFunction::ResponseAction
WebrtcLoggingPrivateStopRtpDumpFunction::Run() {
  std::optional<StopRtpDump::Params> params =
      StopRtpDump::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->incoming && !params->outgoing) {
    FireCallback(false, "Either incoming or outgoing must be true.");
    return AlreadyResponded();
  }

  RtpDumpType type =
      (params->incoming && params->outgoing)
          ? RTP_DUMP_BOTH
          : (params->incoming ? RTP_DUMP_INCOMING : RTP_DUMP_OUTGOING);

  std::string error;
  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin, &error);
  if (!host) {
    return RespondNow(Error(std::move(error)));
  }

  WebRtcLoggingController* webrtc_logging_controller =
      WebRtcLoggingController::FromRenderProcessHost(host);

  WebRtcLoggingController::GenericDoneCallback callback = base::BindOnce(
      &WebrtcLoggingPrivateStopRtpDumpFunction::FireCallback, this);

  webrtc_logging_controller->StopRtpDump(type, std::move(callback));
  return RespondLater();
}

ExtensionFunction::ResponseAction
WebrtcLoggingPrivateStartAudioDebugRecordingsFunction::Run() {
  if (!CanEnableAudioDebugRecordingsFromExtension(extension())) {
    return RespondNow(Error(""));
  }

  std::optional<StartAudioDebugRecordings::Params> params =
      StartAudioDebugRecordings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->seconds < 0) {
    FireErrorCallback("seconds must be greater than or equal to 0");
    return AlreadyResponded();
  }

  std::string error;
  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin, &error);
  if (!host) {
    return RespondNow(Error(std::move(error)));
  }

  scoped_refptr<AudioDebugRecordingsHandler> audio_debug_recordings_handler(
      base::UserDataAdapter<AudioDebugRecordingsHandler>::Get(
          host, AudioDebugRecordingsHandler::kAudioDebugRecordingsHandlerKey));

  audio_debug_recordings_handler->StartAudioDebugRecordings(
      host, base::Seconds(params->seconds),
      base::BindOnce(
          &WebrtcLoggingPrivateStartAudioDebugRecordingsFunction::FireCallback,
          this),
      base::BindOnce(&WebrtcLoggingPrivateStartAudioDebugRecordingsFunction::
                         FireErrorCallback,
                     this));
  return RespondLater();
}

ExtensionFunction::ResponseAction
WebrtcLoggingPrivateStopAudioDebugRecordingsFunction::Run() {
  if (!CanEnableAudioDebugRecordingsFromExtension(extension())) {
    return RespondNow(Error(""));
  }

  std::optional<StopAudioDebugRecordings::Params> params =
      StopAudioDebugRecordings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin, &error);
  if (!host) {
    return RespondNow(Error(std::move(error)));
  }

  scoped_refptr<AudioDebugRecordingsHandler> audio_debug_recordings_handler(
      base::UserDataAdapter<AudioDebugRecordingsHandler>::Get(
          host, AudioDebugRecordingsHandler::kAudioDebugRecordingsHandlerKey));

  audio_debug_recordings_handler->StopAudioDebugRecordings(
      host,
      base::BindOnce(
          &WebrtcLoggingPrivateStopAudioDebugRecordingsFunction::FireCallback,
          this),
      base::BindOnce(&WebrtcLoggingPrivateStopAudioDebugRecordingsFunction::
                         FireErrorCallback,
                     this));
  return RespondLater();
}

ExtensionFunction::ResponseAction
WebrtcLoggingPrivateStartEventLoggingFunction::Run() {
  std::optional<StartEventLogging::Params> params =
      StartEventLogging::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin, &error);
  if (!host) {
    return RespondNow(Error(std::move(error)));
  }

  WebRtcLoggingController* webrtc_logging_controller =
      WebRtcLoggingController::FromRenderProcessHost(host);
  if (!webrtc_logging_controller) {
    return RespondNow(Error("WebRTC logging controller not found."));
  }

  WebRtcLoggingController::StartEventLoggingCallback callback =
      base::BindRepeating(
          &WebrtcLoggingPrivateStartEventLoggingFunction::FireCallback, this);

  webrtc_logging_controller->StartEventLogging(
      params->session_id, params->max_log_size_bytes, params->output_period_ms,
      params->web_app_id, callback);
  return RespondLater();
}

void WebrtcLoggingPrivateStartEventLoggingFunction::FireCallback(
    bool success,
    const std::string& log_id,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (success) {
    DCHECK(!log_id.empty());
    DCHECK(error_message.empty());
    api::webrtc_logging_private::StartEventLoggingResult result;
    result.log_id = log_id;
    Respond(WithArguments(result.ToValue()));
  } else {
    DCHECK(log_id.empty());
    DCHECK(!error_message.empty());
    Respond(Error(error_message));
  }
}

ExtensionFunction::ResponseAction
WebrtcLoggingPrivateGetLogsDirectoryFunction::Run() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Unlike other WebrtcLoggingPrivate functions that take a RequestInfo object,
  // this function shouldn't be called by a component extension on behalf of
  // some web code. It returns a DirectoryEntry for use directly in the calling
  // app or extension. Consequently, this function should run with the
  // extension's render process host, since that is the caller's render process
  // that should be granted access to the logs directory.
  content::RenderProcessHost* host = render_frame_host()->GetProcess();

  WebRtcLoggingController* webrtc_logging_controller =
      WebRtcLoggingController::FromRenderProcessHost(host);
  if (!webrtc_logging_controller) {
    FireErrorCallback("WebRTC logging controller not found.");
    return AlreadyResponded();
  }

  webrtc_logging_controller->GetLogsDirectory(
      base::BindOnce(
          &WebrtcLoggingPrivateGetLogsDirectoryFunction::FireCallback, this),
      base::BindOnce(
          &WebrtcLoggingPrivateGetLogsDirectoryFunction::FireErrorCallback,
          this));
  return RespondLater();
#else   // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return RespondNow(Error("Not supported on the current OS"));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

void WebrtcLoggingPrivateGetLogsDirectoryFunction::FireCallback(
    const std::string& filesystem_id,
    const std::string& base_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value::Dict dict;
  dict.Set("fileSystemId", filesystem_id);
  dict.Set("baseName", base_name);
  Respond(WithArguments(std::move(dict)));
}

void WebrtcLoggingPrivateGetLogsDirectoryFunction::FireErrorCallback(
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Respond(Error(error_message));
}

}  // namespace extensions
