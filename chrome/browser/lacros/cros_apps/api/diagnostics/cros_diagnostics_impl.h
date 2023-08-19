// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CROS_APPS_API_DIAGNOSTICS_CROS_DIAGNOSTICS_IMPL_H_
#define CHROME_BROWSER_LACROS_CROS_APPS_API_DIAGNOSTICS_CROS_DIAGNOSTICS_IMPL_H_

#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
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

 private:
  friend class content::DocumentUserData<CrosDiagnosticsImpl>;

  CrosDiagnosticsImpl(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::CrosDiagnostics> receiver);

  DOCUMENT_USER_DATA_KEY_DECL();

  mojo::Receiver<blink::mojom::CrosDiagnostics> receiver_;
};

#endif  // CHROME_BROWSER_LACROS_CROS_APPS_API_DIAGNOSTICS_CROS_DIAGNOSTICS_IMPL_H_
