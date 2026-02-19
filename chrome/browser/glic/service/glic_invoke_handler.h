// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"

namespace glic {

class GlicInstanceImpl;

// Handles an invocation of Glic, parsing options and communicating with the
// instance's host.
class GlicInvokeHandler {
 public:
  using InvokeCompleteCallback =
      base::OnceCallback<void(const InstanceId&, GlicInvokeHandler*)>;

  GlicInvokeHandler(GlicInstanceImpl& instance,
                    GlicInvokeOptions options,
                    InvokeCompleteCallback invoke_complete_callback);
  ~GlicInvokeHandler();

  GlicInvokeHandler(const GlicInvokeHandler&) = delete;
  GlicInvokeHandler& operator=(const GlicInvokeHandler&) = delete;

 private:
  void SendToClient();
  mojom::InvokeOptionsPtr CreateMojoOptions();
  void OnSendToClientComplete(base::OnceClosure callback);

  const base::raw_ref<GlicInstanceImpl> instance_;
  GlicInvokeOptions options_;
  InvokeCompleteCallback invoke_complete_callback_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_
