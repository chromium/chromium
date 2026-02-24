// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"

namespace glic {

class GlicInstanceImpl;

// Handles an invocation of Glic, parsing options and communicating with the
// instance's host.
class GlicInvokeHandler {
 public:
  using CompletionCallback =
      base::OnceCallback<void(GlicInstance*, GlicInvokeHandler*)>;

  GlicInvokeHandler(GlicInstanceImpl& instance,
                    GlicInvokeOptions options,
                    CompletionCallback completion_callback);
  ~GlicInvokeHandler();

  GlicInvokeHandler(const GlicInvokeHandler&) = delete;
  GlicInvokeHandler& operator=(const GlicInvokeHandler&) = delete;

  // Kicks off the invocation process.
  void Invoke();

  // Ends the invocation process with the given error.
  void OnError(GlicInvokeError error);

 private:
  void SendToClient();
  mojom::InvokeOptionsPtr CreateMojoOptions();
  void OnSuccess();

  const base::raw_ref<GlicInstanceImpl> instance_;
  GlicInvokeOptions options_;
  CompletionCallback completion_callback_;

  base::WeakPtrFactory<GlicInvokeHandler> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_
