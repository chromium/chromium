// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FLASH_BROWSER_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FLASH_BROWSER_HOST_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

namespace base {
class Time;
}

namespace content {
class BrowserPpapiHost;
}

namespace content_settings {
class CookieSettings;
}

class GURL;

class PepperFlashBrowserHost : public ppapi::host::ResourceHost {
 public:
  PepperFlashBrowserHost(content::BrowserPpapiHost* host,
                         PP_Instance instance,
                         PP_Resource resource);
  ~PepperFlashBrowserHost() override;

  // ppapi::host::ResourceHost override.
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

 private:
  void OnDelayTimerFired();
  int32_t OnUpdateActivity(ppapi::host::HostMessageContext* host_context);
  int32_t OnGetLocalTimeZoneOffset(
      ppapi::host::HostMessageContext* host_context,
      const base::Time& t);
  int32_t OnGetLocalDataRestrictions(ppapi::host::HostMessageContext* context);

  void GetLocalDataRestrictions(
      ppapi::host::ReplyMessageContext reply_context,
      const GURL& document_url,
      const GURL& plugin_url,
      scoped_refptr<content_settings::CookieSettings> cookie_settings);

  device::mojom::WakeLock* GetWakeLock();

  content::BrowserPpapiHost* host_;
  int render_process_id_;

  // Requests a wake lock to prevent going to sleep, and a timer to cancel it
  // after a certain amount of time has elapsed without an UpdateActivity.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;
  base::DelayTimer delay_timer_;

  // For fetching the Flash LSO settings.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  base::WeakPtrFactory<PepperFlashBrowserHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PepperFlashBrowserHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FLASH_BROWSER_HOST_H_
