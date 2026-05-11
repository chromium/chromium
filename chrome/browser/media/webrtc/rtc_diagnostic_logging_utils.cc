// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/rtc_diagnostic_logging_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_id.h"
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const std::optional<WebRtcLoggingController::WebApiSettings>& settings =
      controller->web_api_settings();
  return settings.has_value() && settings->origin.IsSameOriginWith(origin);
}

WebRtcLoggingController* GetControllerAndVerifySettings(
    content::RenderFrameHost& frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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

bool IsDiagnosticEventLogCollectionAllowed(
    content::RenderFrameHost& frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const Profile* profile =
      Profile::FromBrowserContext(frame_host.GetBrowserContext());
  if (!profile) {
    return false;
  }
  if (profile->IsOffTheRecord()) {
    return false;
  }
  const PrefService* prefs = profile->GetPrefs();
  if (!prefs->GetBoolean(prefs::kWebRtcEventLogCollectionAllowed)) {
    return false;
  }
  const url::Origin& origin = frame_host.GetLastCommittedOrigin();
  if (origin.opaque()) {
    return false;
  }

  const GURL url = origin.GetURL();
  for (const base::Value& value :
       prefs->GetList(prefs::kWebRTCDiagnosticLogCollectionAllowedForOrigins)) {
    if (value.is_string()) {
      ContentSettingsPattern pattern =
          ContentSettingsPattern::FromString(value.GetString());
      if (pattern.IsValid() && pattern.Matches(url)) {
        return true;
      }
    }
  }

  return false;
}

void DoFinishRtcDiagnosticLogging(
    scoped_refptr<WebRtcLoggingController> controller,
    const url::Origin origin,
    content::ChildProcessId process_id,
    base::OnceClosure callback,
    bool /*set_metadata_success*/,
    const std::string& /*set_metadata_error*/) {
  base::RepeatingClosure barrier = base::BarrierClosure(2, std::move(callback));
  // Stop event logging.
  if (auto* manager =
          ::webrtc_event_logging::WebRtcEventLogManager::GetInstance()) {
    manager->FinishLogging(process_id.value(), barrier);
  } else {
    barrier.Run();
  }

  // Stop text logging.
  controller->StopLogging(base::BindOnce(
      [](scoped_refptr<WebRtcLoggingController> controller,
         const url::Origin origin, base::RepeatingClosure barrier, bool success,
         const std::string& error) {
        if (success && VerifySettings(controller.get(), origin) &&
            !controller->web_api_settings()->should_upload_on_stop) {
          std::string log_id = base::NumberToString(
              base::Time::Now().InSecondsFSinceUnixEpoch());
          controller->StoreLog(
              log_id, base::BindOnce([](base::RepeatingClosure b, bool,
                                        const std::string&) { b.Run(); },
                                     std::move(barrier)));
        } else {
          barrier.Run();
        }
      },
      controller, origin, std::move(barrier)));
}
#endif

}  // namespace

void StartRtcDiagnosticLogging(
    content::RenderFrameHost& frame_host,
    bool should_upload_on_stop,
    const base::flat_map<std::string, std::string>& metadata,
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();

#if WEBRTC_DIAGNOSTIC_LOGGING_SUPPORTED
  url::Origin origin = frame_host.GetLastCommittedOrigin();
  content::RenderFrameHost* main_frame_host = frame_host.GetMainFrame();
  if (!main_frame_host ||
      !main_frame_host->GetLastCommittedOrigin().IsSameOriginWith(origin) ||
      !main_frame_host->IsInPrimaryMainFrame()) {
    std::move(callback).Run(uuid);
    return;
  }
  content::BrowserContext* browser_context = frame_host.GetBrowserContext();
  if (!WebRtcLoggingController::IsWebRtcTextLogAllowed(
          browser_context, webrtc_logging::ApiType::kWeb, origin)) {
    std::move(callback).Run(uuid);
    return;
  }
  content::RenderProcessHost* process_host = frame_host.GetProcess();
  if (!process_host) {
    std::move(callback).Run(uuid);
    return;
  }
  auto* controller =
      WebRtcLoggingController::FromRenderProcessHost(process_host);
  if (!controller) {
    std::move(callback).Run(uuid);
    return;
  }

  auto metadata_map =
      std::make_unique<WebRtcLogMetaDataMap>(metadata.begin(), metadata.end());
  metadata_map->emplace("__uuid__", uuid);

  WebRtcLoggingController::WebApiSettings web_api_settings{
      .should_upload_on_stop = should_upload_on_stop,
      .origin = origin,
      .uuid = uuid};

  controller->StartLogging(
      base::BindOnce(
          [](scoped_refptr<WebRtcLoggingController> controller,
             std::unique_ptr<WebRtcLogMetaDataMap> metadata,
             const url::Origin origin, std::string uuid,
             base::OnceCallback<void(const std::string&)> callback,
             bool success, const std::string&) {
            if (success && VerifySettings(controller.get(), origin)) {
              controller->SetMetaData(
                  std::move(metadata),
                  base::BindOnce(
                      [](std::string uuid,
                         base::OnceCallback<void(const std::string&)> callback,
                         bool,
                         const std::string&) { std::move(callback).Run(uuid); },
                      std::move(uuid), std::move(callback)));
            } else {
              std::move(callback).Run(uuid);
            }
          },
          base::WrapRefCounted(controller), std::move(metadata_map), origin,
          std::move(uuid), std::move(callback)),
      web_api_settings);
#else
  std::move(callback).Run(uuid);
#endif
}

void FinishRtcDiagnosticLogging(
    content::RenderFrameHost& frame_host,
    const base::flat_map<std::string, std::string>& metadata,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if WEBRTC_DIAGNOSTIC_LOGGING_SUPPORTED
  auto* controller = GetControllerAndVerifySettings(frame_host);
  if (!controller) {
    std::move(callback).Run();
    return;
  }

  auto metadata_map =
      std::make_unique<WebRtcLogMetaDataMap>(metadata.begin(), metadata.end());
  metadata_map->erase("__uuid__");

  const url::Origin origin = frame_host.GetLastCommittedOrigin();

  controller->SetMetaData(
      std::move(metadata_map),
      base::BindOnce(&DoFinishRtcDiagnosticLogging,
                     base::WrapRefCounted(controller), origin,
                     frame_host.GetProcess()->GetID(), std::move(callback)));

#else
  std::move(callback).Run();
#endif
}

void CancelRtcDiagnosticLogging(content::RenderFrameHost& frame_host,
                                base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if WEBRTC_DIAGNOSTIC_LOGGING_SUPPORTED
  auto* controller = GetControllerAndVerifySettings(frame_host);
  if (!controller) {
    std::move(callback).Run();
    return;
  }

  base::RepeatingClosure barrier = base::BarrierClosure(2, std::move(callback));
  if (auto* manager =
          ::webrtc_event_logging::WebRtcEventLogManager::GetInstance()) {
    manager->CancelLogging(frame_host.GetProcess()->GetID().value(),
                           controller->web_api_settings()->uuid, barrier);
  } else {
    barrier.Run();
  }

  controller->set_upload_log_on_render_close(false);
  controller->set_should_upload_on_stop(false);
  controller->StopLogging(base::BindOnce(
      [](scoped_refptr<WebRtcLoggingController> controller,
         base::RepeatingClosure barrier, bool success,
         const std::string& error) {
        controller->DiscardLog(base::BindOnce(
            [](base::RepeatingClosure barrier, bool success,
               const std::string& error) { std::move(barrier).Run(); },
            std::move(barrier)));
      },
      base::WrapRefCounted(controller), std::move(barrier)));
#else
  std::move(callback).Run();
#endif
}

void StartRtcPeerConnectionEventDiagnosticLogging(
    content::RenderFrameHost& frame_host,
    const std::string& session_id,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if WEBRTC_DIAGNOSTIC_LOGGING_SUPPORTED
  if (!IsDiagnosticEventLogCollectionAllowed(frame_host)) {
    std::move(callback).Run();
    return;
  }
  WebRtcLoggingController* controller =
      GetControllerAndVerifySettings(frame_host);
  if (!controller) {
    std::move(callback).Run();
    return;
  }

  static constexpr size_t kMaxLogSize = 1024 * 1024 * 4;
  static constexpr int kOutputPeriodMs = 1000;
  // Use different app_ids for same site vs cross site.
  const size_t web_app_id =
      webrtc_event_logging::IsOriginSameSiteWithUploadEndpoint(
          frame_host.GetLastCommittedOrigin())
          ? webrtc_event_logging::kSameSiteWebAppId
          : webrtc_event_logging::kCrossSiteWebAppId;

  controller->StartEventLogging(
      webrtc_logging::ApiType::kWeb, session_id, kMaxLogSize, kOutputPeriodMs,
      web_app_id,
      base::BindOnce(
          [](base::OnceClosure callback, bool success,
             const std::string& log_id,
             const std::string& error_message) { std::move(callback).Run(); },
          std::move(callback)));
#else
  std::move(callback).Run();
#endif
}

}  // namespace rtc_diagnostic_logging
