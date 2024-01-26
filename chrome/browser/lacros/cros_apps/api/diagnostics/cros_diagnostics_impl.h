// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CROS_APPS_API_DIAGNOSTICS_CROS_DIAGNOSTICS_IMPL_H_
#define CHROME_BROWSER_LACROS_CROS_APPS_API_DIAGNOSTICS_CROS_DIAGNOSTICS_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/network_interfaces.h"
#include "third_party/blink/public/mojom/chromeos/diagnostics/cros_diagnostics.mojom.h"

namespace content {
class RenderFrameHost;
}

class CrosDiagnosticsImpl
    : public blink::mojom::CrosDiagnostics,
      public content::DocumentUserData<CrosDiagnosticsImpl> {
 public:
  ~CrosDiagnosticsImpl() override;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::CrosDiagnostics> receiver);

  // blink::mojom::CrosDiagnostics
  void GetCpuInfo(GetCpuInfoCallback callback) override;
  void GetNetworkInterfaces(GetNetworkInterfacesCallback callback) override;

 private:
  friend class content::DocumentUserData<CrosDiagnosticsImpl>;

  CrosDiagnosticsImpl(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::CrosDiagnostics> receiver);

  void GetCpuInfoProbeTelemetryInfoCallback(
      GetCpuInfoCallback callback,
      blink::mojom::CrosCpuInfoPtr cpu_info_mojom,
      crosapi::mojom::ProbeTelemetryInfoPtr telemetry_info);

  void GetNetworkInterfacesGetNetworkListCallback(
      GetNetworkInterfacesCallback callback,
      const std::optional<net::NetworkInterfaceList>& interface_list);

  DOCUMENT_USER_DATA_KEY_DECL();

  mojo::Receiver<blink::mojom::CrosDiagnostics> cros_diagnostics_receiver_;

  // Last member definition. Needed here because WeakPtrFactory members which
  // refer to their outer class must be the last member in the outer class
  // definition.
  base::WeakPtrFactory<CrosDiagnosticsImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_CROS_APPS_API_DIAGNOSTICS_CROS_DIAGNOSTICS_IMPL_H_
