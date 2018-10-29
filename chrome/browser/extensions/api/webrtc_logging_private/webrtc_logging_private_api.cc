// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webrtc_logging_private/webrtc_logging_private_api.h"

#include <memory>

#include "base/command_line.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
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

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "extensions/common/permissions/permissions_data.h"
#endif

namespace {

bool CanEnableAudioDebugRecordingsFromExtension(
    const extensions::Extension* extension) {
  bool enabled_by_permissions = false;
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  if (extension) {
    enabled_by_permissions =
        extension->permissions_data()->active_permissions().HasAPIPermission(
            extensions::APIPermission::kWebrtcLoggingPrivateAudioDebug);
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
  return base::UintToString(base::Hash(security_origin + log_id));
}
}  // namespace

// TODO(hlundin): Consolidate with WebrtcAudioPrivateFunction and improve.
// http://crbug.com/710371
content::RenderProcessHost* WebrtcLoggingPrivateFunction::RphFromRequest(
    const api::webrtc_logging_private::RequestInfo& request,
    const std::string& security_origin) {
  // There are 2 ways these API functions can get called.
  //
  //  1. From a whitelisted component extension on behalf of a page with the
  //  appropriate origin via a message from that page. In this case, either the
  //  guest process id or the tab id is on the message received by the component
  //  extension, and the extension can pass that along in RequestInfo as
  //  |guest_process_id| or |tab_id|.
  //
  //  2. From a whitelisted app that hosts a page in a webview. In this case,
  //  the app should call these API functions with the |target_webview| flag
  //  set, from a web contents that has exactly 1 webview .

  // If |target_webview| is set, lookup the guest content's render process in
  // the sender's web contents. There should be exactly 1 guest.
  if (request.target_webview.get()) {
    content::RenderProcessHost* target_host = nullptr;
    int guests_found = 0;
    auto get_guest = [](int* guests_found,
                        content::RenderProcessHost** target_host,
                        content::WebContents* guest_contents) {
      *guests_found = *guests_found + 1;
      *target_host = guest_contents->GetMainFrame()->GetProcess();
      // Don't short-circuit, so we can count how many other guest contents
      // there are.
      return false;
    };
    guest_view::GuestViewManager::FromBrowserContext(browser_context())
        ->ForEachGuest(
            GetSenderWebContents(),
            base::BindRepeating(get_guest, &guests_found, &target_host));
    if (!target_host) {
      SetError("No webview render process found");
      return nullptr;
    }
    if (guests_found > 1) {
      SetError("Multiple webviews found");
      return nullptr;
    }
    return target_host;
  }

  // If |guest_process_id| is defined, directly use this id to find the
  // corresponding RenderProcessHost.
  if (request.guest_process_id.get()) {
    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(*request.guest_process_id);
    if (!rph) {
      SetError(
          base::StringPrintf("Failed to get RPH fro guest proccess ID (%d).",
                             *request.guest_process_id));
    }
    return rph;
  }

  // Otherwise, use the |tab_id|. If there's no |target_viewview|, no |tab_id|,
  // and no |guest_process_id|, we can't look up the RenderProcessHost.
  if (!request.tab_id.get()) {
    SetError("No webview, tab ID, or guest process ID specified.");
    return nullptr;
  }

  int tab_id = *request.tab_id;
  content::WebContents* contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(tab_id, GetProfile(), true, nullptr,
                                    nullptr, &contents, nullptr)) {
    SetError(extensions::ErrorUtils::FormatErrorMessage(
        extensions::tabs_constants::kTabNotFoundError,
        base::IntToString(tab_id)));
    return nullptr;
  }
  if (!contents) {
    SetError("Web contents for tab not found.");
    return nullptr;
  }
  GURL expected_origin = contents->GetLastCommittedURL().GetOrigin();
  if (expected_origin.spec() != security_origin) {
    SetError(base::StringPrintf(
        "Invalid security origin. Expected=%s, actual=%s",
        expected_origin.spec().c_str(), security_origin.c_str()));
    return nullptr;
  }

  content::RenderProcessHost* rph = contents->GetMainFrame()->GetProcess();
  if (!rph) {
    SetError("Failed to get RPH.");
  }
  return rph;
}

scoped_refptr<WebRtcLoggingHandlerHost>
WebrtcLoggingPrivateFunction::LoggingHandlerFromRequest(
    const api::webrtc_logging_private::RequestInfo& request,
    const std::string& security_origin) {
  content::RenderProcessHost* host = RphFromRequest(request, security_origin);
  if (!host) {
    // SetError() will have been called by RphFromRequest().
    return nullptr;
  }

  return base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(
      host, WebRtcLoggingHandlerHost::kWebRtcLoggingHandlerHostKey);
}

scoped_refptr<WebRtcLoggingHandlerHost>
WebrtcLoggingPrivateFunctionWithGenericCallback::PrepareTask(
    const api::webrtc_logging_private::RequestInfo& request,
    const std::string& security_origin,
    WebRtcLoggingHandlerHost::GenericDoneCallback* callback) {
  *callback = base::Bind(
      &WebrtcLoggingPrivateFunctionWithGenericCallback::FireCallback, this);
  return LoggingHandlerFromRequest(request, security_origin);
}

void WebrtcLoggingPrivateFunctionWithGenericCallback::FireCallback(
    bool success, const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success)
    SetError(error_message);
  SendResponse(success);
}

void WebrtcLoggingPrivateFunctionWithUploadCallback::FireCallback(
    bool success, const std::string& report_id,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (success) {
    api::webrtc_logging_private::UploadResult result;
    result.report_id = report_id;
    SetResult(result.ToValue());
  } else {
    SetError(error_message);
  }
  SendResponse(success);
}

void WebrtcLoggingPrivateFunctionWithRecordingDoneCallback::FireErrorCallback(
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetError(error_message);
  SendResponse(false);
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
  SetResult(result.ToValue());
  SendResponse(true);
}

bool WebrtcLoggingPrivateSetMetaDataFunction::RunAsync() {
  std::unique_ptr<SetMetaData::Params> params(
      SetMetaData::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebRtcLoggingHandlerHost::GenericDoneCallback callback;
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host =
      PrepareTask(params->request, params->security_origin, &callback);
  if (!webrtc_logging_handler_host.get())
    return false;

  std::unique_ptr<MetaDataMap> meta_data(new MetaDataMap());
  for (const MetaDataEntry& entry : params->meta_data)
    (*meta_data)[entry.key] = entry.value;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&WebRtcLoggingHandlerHost::SetMetaData,
                     webrtc_logging_handler_host, std::move(meta_data),
                     callback));

  return true;
}

bool WebrtcLoggingPrivateStartFunction::RunAsync() {
  std::unique_ptr<Start::Params> params(Start::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebRtcLoggingHandlerHost::GenericDoneCallback callback;
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host =
      PrepareTask(params->request, params->security_origin, &callback);
  if (!webrtc_logging_handler_host.get())
    return false;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&WebRtcLoggingHandlerHost::StartLogging,
                     webrtc_logging_handler_host, callback));

  return true;
}

bool WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::RunAsync() {
  std::unique_ptr<SetUploadOnRenderClose::Params> params(
      SetUploadOnRenderClose::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      LoggingHandlerFromRequest(params->request, params->security_origin));
  if (!webrtc_logging_handler_host.get())
    return false;

  webrtc_logging_handler_host->set_upload_log_on_render_close(
      params->should_upload);

  // Post a task since this is an asynchronous extension function.
  // TODO(devlin): This is unneccessary; this should just be a
  // UIThreadExtensionFunction. Fix this.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::SendResponse,
          this, true));
  return true;
}

bool WebrtcLoggingPrivateStopFunction::RunAsync() {
  std::unique_ptr<Stop::Params> params(Stop::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebRtcLoggingHandlerHost::GenericDoneCallback callback;
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host =
      PrepareTask(params->request, params->security_origin, &callback);
  if (!webrtc_logging_handler_host.get())
    return false;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&WebRtcLoggingHandlerHost::StopLogging,
                     webrtc_logging_handler_host, callback));

  return true;
}

bool WebrtcLoggingPrivateStoreFunction::RunAsync() {
  std::unique_ptr<Store::Params> params(Store::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebRtcLoggingHandlerHost::GenericDoneCallback callback;
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host =
      PrepareTask(params->request, params->security_origin, &callback);
  if (!webrtc_logging_handler_host.get())
    return false;

  const std::string local_log_id(HashIdWithOrigin(params->security_origin,
                                                  params->log_id));

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&WebRtcLoggingHandlerHost::StoreLog,
                     webrtc_logging_handler_host, local_log_id, callback));

  return true;
}

bool WebrtcLoggingPrivateUploadStoredFunction::RunAsync() {
  std::unique_ptr<UploadStored::Params> params(
      UploadStored::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<WebRtcLoggingHandlerHost> logging_handler(
      LoggingHandlerFromRequest(params->request, params->security_origin));
  if (!logging_handler.get())
    return false;

  WebRtcLoggingHandlerHost::UploadDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateUploadStoredFunction::FireCallback, this);

  const std::string local_log_id(HashIdWithOrigin(params->security_origin,
                                                  params->log_id));

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&WebRtcLoggingHandlerHost::UploadStoredLog,
                     logging_handler, local_log_id, callback));

  return true;
}

bool WebrtcLoggingPrivateUploadFunction::RunAsync() {
  std::unique_ptr<Upload::Params> params(Upload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<WebRtcLoggingHandlerHost> logging_handler(
      LoggingHandlerFromRequest(params->request, params->security_origin));
  if (!logging_handler.get())
    return false;

  WebRtcLoggingHandlerHost::UploadDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateUploadFunction::FireCallback, this);

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&WebRtcLoggingHandlerHost::UploadLog,
                                          logging_handler, callback));

  return true;
}

bool WebrtcLoggingPrivateDiscardFunction::RunAsync() {
  std::unique_ptr<Discard::Params> params(Discard::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebRtcLoggingHandlerHost::GenericDoneCallback callback;
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host =
      PrepareTask(params->request, params->security_origin, &callback);
  if (!webrtc_logging_handler_host.get())
    return false;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&WebRtcLoggingHandlerHost::DiscardLog,
                     webrtc_logging_handler_host, callback));

  return true;
}

bool WebrtcLoggingPrivateStartRtpDumpFunction::RunAsync() {
  std::unique_ptr<StartRtpDump::Params> params(
      StartRtpDump::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->incoming && !params->outgoing) {
    FireCallback(false, "Either incoming or outgoing must be true.");
    return true;
  }

  RtpDumpType type =
      (params->incoming && params->outgoing)
          ? RTP_DUMP_BOTH
          : (params->incoming ? RTP_DUMP_INCOMING : RTP_DUMP_OUTGOING);

  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin);
  if (!host) {
    // SetError() will have been called by RphFromRequest().
    return false;
  }

  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(
          host, WebRtcLoggingHandlerHost::kWebRtcLoggingHandlerHostKey));

  WebRtcLoggingHandlerHost::GenericDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateStartRtpDumpFunction::FireCallback, this);

  // This call cannot fail.
  content::RenderProcessHost::WebRtcStopRtpDumpCallback stop_callback =
      host->StartRtpDump(params->incoming,
                         params->outgoing,
                         base::Bind(&WebRtcLoggingHandlerHost::OnRtpPacket,
                                    webrtc_logging_handler_host));

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&WebRtcLoggingHandlerHost::StartRtpDump,
                     webrtc_logging_handler_host, type, callback,
                     stop_callback));
  return true;
}

bool WebrtcLoggingPrivateStopRtpDumpFunction::RunAsync() {
  std::unique_ptr<StopRtpDump::Params> params(
      StopRtpDump::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->incoming && !params->outgoing) {
    FireCallback(false, "Either incoming or outgoing must be true.");
    return true;
  }

  RtpDumpType type =
      (params->incoming && params->outgoing)
          ? RTP_DUMP_BOTH
          : (params->incoming ? RTP_DUMP_INCOMING : RTP_DUMP_OUTGOING);

  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin);
  if (!host) {
    // SetError() will have been called by RphFromRequest().
    return false;
  }

  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(
          host, WebRtcLoggingHandlerHost::kWebRtcLoggingHandlerHostKey));

  WebRtcLoggingHandlerHost::GenericDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateStopRtpDumpFunction::FireCallback, this);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&WebRtcLoggingHandlerHost::StopRtpDump,
                     webrtc_logging_handler_host, type, callback));
  return true;
}

bool WebrtcLoggingPrivateStartAudioDebugRecordingsFunction::RunAsync() {
  if (!CanEnableAudioDebugRecordingsFromExtension(extension())) {
    return false;
  }

  std::unique_ptr<StartAudioDebugRecordings::Params> params(
      StartAudioDebugRecordings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->seconds < 0) {
    FireErrorCallback("seconds must be greater than or equal to 0");
    return true;
  }

  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin);
  if (!host) {
    // SetError() will have been called by RphFromRequest().
    return false;
  }

  scoped_refptr<AudioDebugRecordingsHandler> audio_debug_recordings_handler(
      base::UserDataAdapter<AudioDebugRecordingsHandler>::Get(
          host, AudioDebugRecordingsHandler::kAudioDebugRecordingsHandlerKey));

  audio_debug_recordings_handler->StartAudioDebugRecordings(
      host, base::TimeDelta::FromSeconds(params->seconds),
      base::Bind(
          &WebrtcLoggingPrivateStartAudioDebugRecordingsFunction::FireCallback,
          this),
      base::Bind(&WebrtcLoggingPrivateStartAudioDebugRecordingsFunction::
                     FireErrorCallback,
                 this));
  return true;
}

bool WebrtcLoggingPrivateStopAudioDebugRecordingsFunction::RunAsync() {
  if (!CanEnableAudioDebugRecordingsFromExtension(extension())) {
    return false;
  }

  std::unique_ptr<StopAudioDebugRecordings::Params> params(
      StopAudioDebugRecordings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin);
  if (!host) {
    // SetError() will have been called by RphFromRequest().
    return false;
  }

  scoped_refptr<AudioDebugRecordingsHandler> audio_debug_recordings_handler(
      base::UserDataAdapter<AudioDebugRecordingsHandler>::Get(
          host, AudioDebugRecordingsHandler::kAudioDebugRecordingsHandlerKey));

  audio_debug_recordings_handler->StopAudioDebugRecordings(
      host,
      base::Bind(
          &WebrtcLoggingPrivateStopAudioDebugRecordingsFunction::FireCallback,
          this),
      base::Bind(&WebrtcLoggingPrivateStopAudioDebugRecordingsFunction::
                     FireErrorCallback,
                 this));
  return true;
}

bool WebrtcLoggingPrivateStartEventLoggingFunction::RunAsync() {
  std::unique_ptr<StartEventLogging::Params> params(
      StartEventLogging::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin);
  if (!host) {
    // SetError() will have been called by RphFromRequest().
    return false;
  }

  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(
          host, WebRtcLoggingHandlerHost::kWebRtcLoggingHandlerHostKey));
  if (!webrtc_logging_handler_host.get()) {
    SetError("WebRTC logging handler not found.");
    return false;
  }

  WebRtcLoggingHandlerHost::StartEventLoggingCallback callback =
      base::BindRepeating(
          &WebrtcLoggingPrivateStartEventLoggingFunction::FireCallback, this);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&WebRtcLoggingHandlerHost::StartEventLogging,
                     webrtc_logging_handler_host, params->peer_connection_id,
                     params->max_log_size_bytes, params->web_app_id, callback));

  return true;
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
    SetResult(result.ToValue());
  } else {
    DCHECK(log_id.empty());
    DCHECK(!error_message.empty());
    SetError(error_message);
  }
  SendResponse(success);
}

bool WebrtcLoggingPrivateGetLogsDirectoryFunction::RunAsync() {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Unlike other WebrtcLoggingPrivate functions that take a RequestInfo object,
  // this function shouldn't be called by a component extension on behalf of
  // some web code. It returns a DirectoryEntry for use directly in the calling
  // app or extension. Consequently, this function should run with the
  // extension's render process host, since that is the caller's render process
  // that should be granted access to the logs directory.
  content::RenderProcessHost* host = render_frame_host()->GetProcess();

  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(
          host, WebRtcLoggingHandlerHost::kWebRtcLoggingHandlerHostKey));
  if (!webrtc_logging_handler_host.get()) {
    FireErrorCallback("WebRTC logging handler not found.");
    return true;
  }

  webrtc_logging_handler_host->GetLogsDirectory(
      base::Bind(&WebrtcLoggingPrivateGetLogsDirectoryFunction::FireCallback,
                 this),
      base::Bind(
          &WebrtcLoggingPrivateGetLogsDirectoryFunction::FireErrorCallback,
          this));
  return true;
#else   // defined(OS_LINUX) || defined(OS_CHROMEOS)
  SetError("Not supported on the current OS");
  SendResponse(false);
  return false;
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
}

void WebrtcLoggingPrivateGetLogsDirectoryFunction::FireCallback(
    const std::string& filesystem_id,
    const std::string& base_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("fileSystemId", filesystem_id);
  dict->SetString("baseName", base_name);
  Respond(OneArgument(std::move(dict)));
}

void WebrtcLoggingPrivateGetLogsDirectoryFunction::FireErrorCallback(
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetError(error_message);
  SendResponse(false);
}

}  // namespace extensions
