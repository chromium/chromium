// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/rtc_diagnostic_logging_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#define WEBRTC_DIAGNOSTIC_LOGGING_SUPPORTED 1
#else
#define WEBRTC_DIAGNOSTIC_LOGGING_SUPPORTED 0
#endif

#if WEBRTC_DIAGNOSTIC_LOGGING_SUPPORTED
#include "chrome/browser/media/webrtc/webrtc_logging_controller.h"
#endif

namespace rtc_diagnostic_logging {

namespace {

#if WEBRTC_DIAGNOSTIC_LOGGING_SUPPORTED
bool VerifySettings(WebRtcLoggingController* controller,
                    const url::Origin& origin) {
  const std::optional<WebRtcLoggingController::WebApiSettings>& settings =
      controller->web_api_settings();
  return settings.has_value() && settings->origin.IsSameOriginWith(origin);
}

WebRtcLoggingController* GetControllerAndVerifySettings(
    content::RenderFrameHost& frame_host) {
  content::RenderProcessHost* process_host = frame_host.GetProcess();
  if (!process_host) {
    return nullptr;
  }
  auto* controller =
      WebRtcLoggingController::FromRenderProcessHost(process_host);
  if (!controller) {
    return nullptr;
  }
  if (!VerifySettings(controller, frame_host.GetLastCommittedOrigin())) {
    return nullptr;
  }
  return controller;
}
#endif

}  // namespace

void StartRtcDiagnosticLogging(
    content::RenderFrameHost& frame_host,
    bool should_upload_on_stop,
    base::flat_map<std::string, std::string> metadata,
    base::OnceCallback<void(const std::string&)> callback) {
  std::string uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::move(callback).Run(uuid);

#if WEBRTC_DIAGNOSTIC_LOGGING_SUPPORTED
  url::Origin origin = frame_host.GetLastCommittedOrigin();
  content::RenderFrameHost* main_frame_host = frame_host.GetMainFrame();
  if (!main_frame_host ||
      !main_frame_host->GetLastCommittedOrigin().IsSameOriginWith(origin) ||
      !main_frame_host->IsInPrimaryMainFrame()) {
    return;
  }
  content::BrowserContext* browser_context = frame_host.GetBrowserContext();
  if (!WebRtcLoggingController::IsWebRtcTextLogAllowed(
          browser_context, webrtc_logging::ApiType::kWeb, origin)) {
    return;
  }
  content::RenderProcessHost* process_host = frame_host.GetProcess();
  if (!process_host) {
    return;
  }
  auto* controller =
      WebRtcLoggingController::FromRenderProcessHost(process_host);
  if (!controller) {
    return;
  }

  auto metadata_map =
      std::make_unique<WebRtcLogMetaDataMap>(metadata.begin(), metadata.end());
  metadata_map->emplace("__uuid__", uuid);

  WebRtcLoggingController::WebApiSettings web_api_settings{
      .should_upload_on_stop = should_upload_on_stop, .origin = origin};

  controller->StartLogging(
      base::BindOnce(
          [](scoped_refptr<WebRtcLoggingController> controller,
             std::unique_ptr<WebRtcLogMetaDataMap> metadata,
             const url::Origin origin, bool success,
             const std::string& error_message) {
            if (success && VerifySettings(controller.get(), origin)) {
              controller->SetMetaData(std::move(metadata), base::DoNothing());
            }
          },
          base::WrapRefCounted(controller), std::move(metadata_map), origin),
      web_api_settings);
#endif
}

void FinishRtcDiagnosticLogging(content::RenderFrameHost& frame_host,
                                base::OnceClosure callback) {
#if WEBRTC_DIAGNOSTIC_LOGGING_SUPPORTED
  auto* controller = GetControllerAndVerifySettings(frame_host);
  if (!controller) {
    std::move(callback).Run();
    return;
  }

  const url::Origin origin = frame_host.GetLastCommittedOrigin();

  controller->StopLogging(base::BindOnce(
      [](scoped_refptr<WebRtcLoggingController> controller,
         const url::Origin origin, base::OnceClosure callback, bool success,
         const std::string& error) {
        std::move(callback).Run();
        if (success && VerifySettings(controller.get(), origin) &&
            !controller->web_api_settings()->should_upload_on_stop) {
          std::string log_id = base::NumberToString(
              base::Time::Now().InSecondsFSinceUnixEpoch());
          controller->StoreLog(log_id, base::DoNothing());
        }
      },
      base::WrapRefCounted(controller), origin, std::move(callback)));
#else
  std::move(callback).Run();
#endif
}

void CancelRtcDiagnosticLogging(content::RenderFrameHost& frame_host,
                                base::OnceClosure callback) {
#if WEBRTC_DIAGNOSTIC_LOGGING_SUPPORTED
  auto* controller = GetControllerAndVerifySettings(frame_host);
  if (!controller) {
    std::move(callback).Run();
    return;
  }

  controller->set_upload_log_on_render_close(false);
  controller->set_should_upload_on_stop(false);
  controller->StopLogging(base::BindOnce(
      [](scoped_refptr<WebRtcLoggingController> controller,
         base::OnceClosure callback, bool success, const std::string& error) {
        controller->DiscardLog(base::BindOnce(
            [](base::OnceClosure callback, bool success,
               const std::string& error) { std::move(callback).Run(); },
            std::move(callback)));
      },
      base::WrapRefCounted(controller), std::move(callback)));
#else
  std::move(callback).Run();
#endif
}

}  // namespace rtc_diagnostic_logging
