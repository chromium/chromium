// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_flash_browser_host.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/system_connector.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#include <CoreServices/CoreServices.h>
#endif

using content::BrowserPpapiHost;
using content::BrowserThread;
using content::RenderProcessHost;

namespace {

// Get the CookieSettings on the UI thread for the given render process ID.
scoped_refptr<content_settings::CookieSettings> GetCookieSettings(
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_id);
  if (render_process_host && render_process_host->GetBrowserContext()) {
    Profile* profile =
        Profile::FromBrowserContext(render_process_host->GetBrowserContext());
    return CookieSettingsFactory::GetForProfile(profile);
  }
  return nullptr;
}

void PepperBindConnectorReceiver(
    mojo::PendingReceiver<service_manager::mojom::Connector>
        connector_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(content::GetSystemConnector());
  content::GetSystemConnector()->BindConnectorReceiver(
      std::move(connector_receiver));
}

}  // namespace

PepperFlashBrowserHost::PepperFlashBrowserHost(BrowserPpapiHost* host,
                                               PP_Instance instance,
                                               PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      host_(host),
      delay_timer_(FROM_HERE,
                   base::TimeDelta::FromSeconds(45),
                   this,
                   &PepperFlashBrowserHost::OnDelayTimerFired) {
  int unused;
  host->GetRenderFrameIDsForInstance(instance, &render_process_id_, &unused);
}

PepperFlashBrowserHost::~PepperFlashBrowserHost() {}

int32_t PepperFlashBrowserHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperFlashBrowserHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_Flash_UpdateActivity,
                                        OnUpdateActivity)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Flash_GetLocalTimeZoneOffset,
                                      OnGetLocalTimeZoneOffset)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_Flash_GetLocalDataRestrictions, OnGetLocalDataRestrictions)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperFlashBrowserHost::OnDelayTimerFired() {
  GetWakeLock()->CancelWakeLock();
}

int32_t PepperFlashBrowserHost::OnUpdateActivity(
    ppapi::host::HostMessageContext* host_context) {
  GetWakeLock()->RequestWakeLock();
  // There is no specification for how long OnUpdateActivity should prevent the
  // screen from going to sleep. Empirically, twitch.tv calls this method every
  // 10 seconds. Be conservative and allow 45 seconds (set in |delay_timer_|'s
  // ctor) before deleting the block.
  delay_timer_.Reset();
  return PP_OK;
}

int32_t PepperFlashBrowserHost::OnGetLocalTimeZoneOffset(
    ppapi::host::HostMessageContext* host_context,
    const base::Time& t) {
  // The reason for this processing being in the browser process is that on
  // Linux, the localtime calls require filesystem access prohibited by the
  // sandbox.
  host_context->reply_msg = PpapiPluginMsg_Flash_GetLocalTimeZoneOffsetReply(
      ppapi::PPGetLocalTimeZoneOffset(t));
  return PP_OK;
}

int32_t PepperFlashBrowserHost::OnGetLocalDataRestrictions(
    ppapi::host::HostMessageContext* context) {
  // Getting the Flash LSO settings requires using the CookieSettings which
  // belong to the profile which lives on the UI thread. We lazily initialize
  // |cookie_settings_| by grabbing the reference from the UI thread and then
  // call |GetLocalDataRestrictions| with it.
  GURL document_url = host_->GetDocumentURLForInstance(pp_instance());
  GURL plugin_url = host_->GetPluginURLForInstance(pp_instance());
  if (cookie_settings_.get()) {
    GetLocalDataRestrictions(context->MakeReplyMessageContext(),
                             document_url,
                             plugin_url,
                             cookie_settings_);
  } else {
    base::PostTaskAndReplyWithResult(
        FROM_HERE, {BrowserThread::UI},
        base::Bind(&GetCookieSettings, render_process_id_),
        base::Bind(&PepperFlashBrowserHost::GetLocalDataRestrictions,
                   weak_factory_.GetWeakPtr(),
                   context->MakeReplyMessageContext(), document_url,
                   plugin_url));
  }
  return PP_OK_COMPLETIONPENDING;
}

void PepperFlashBrowserHost::GetLocalDataRestrictions(
    ppapi::host::ReplyMessageContext reply_context,
    const GURL& document_url,
    const GURL& plugin_url,
    scoped_refptr<content_settings::CookieSettings> cookie_settings) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Lazily initialize |cookie_settings_|. The cookie settings are thread-safe
  // ref-counted so as long as we hold a reference to them we can safely access
  // them on the IO thread.
  if (!cookie_settings_.get()) {
    cookie_settings_ = cookie_settings;
  } else {
    DCHECK(cookie_settings_.get() == cookie_settings.get());
  }

  PP_FlashLSORestrictions restrictions = PP_FLASHLSORESTRICTIONS_NONE;
  if (cookie_settings_.get() && document_url.is_valid() &&
      plugin_url.is_valid()) {
    if (!cookie_settings_->IsCookieAccessAllowed(document_url, plugin_url))
      restrictions = PP_FLASHLSORESTRICTIONS_BLOCK;
    else if (cookie_settings_->IsCookieSessionOnly(plugin_url))
      restrictions = PP_FLASHLSORESTRICTIONS_IN_MEMORY;
  }
  SendReply(reply_context,
            PpapiPluginMsg_Flash_GetLocalDataRestrictionsReply(
                static_cast<int32_t>(restrictions)));
}

device::mojom::WakeLock* PepperFlashBrowserHost::GetWakeLock() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (wake_lock_)
    return wake_lock_.get();

  // The system Connector might be not initialized in some testing environments.
  if (!content::GetSystemConnector())
    return wake_lock_.get();

  mojo::PendingReceiver<service_manager::mojom::Connector> connector_receiver;
  auto connector = service_manager::Connector::Create(&connector_receiver);

  // The existing connector is bound to the UI thread, the current thread is
  // IO thread. So bind the Connector receiver of IO thread to the connector
  // in UI thread.
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&PepperBindConnectorReceiver,
                                std::move(connector_receiver)));

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  connector->Connect(device::mojom::kServiceName,
                     wake_lock_provider.BindNewPipeAndPassReceiver());
  wake_lock_provider->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventDisplaySleep,
      device::mojom::WakeLockReason::kOther, "Requested By PepperFlash",
      wake_lock_.BindNewPipeAndPassReceiver());
  return wake_lock_.get();
}
