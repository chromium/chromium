// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_DIAGNOSTICS_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_DIAGNOSTICS_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/guest_os/guest_os_diagnostics.mojom-forward.h"

namespace plugin_vm {

// Get Plugin VM diagnostics. This is mainly for the the debugging page
// chrome://vm/parallels.
void GetDiagnostics(
    base::OnceCallback<void(guest_os::mojom::DiagnosticsPtr)> callback);

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_DIAGNOSTICS_H_
