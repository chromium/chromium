// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICE_SANDBOX_TYPE_H_
#define CHROME_BROWSER_SERVICE_SANDBOX_TYPE_H_

#include "build/build_config.h"
#include "content/public/browser/service_process_host.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/sandbox_type.h"

#if (defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
     defined(OS_CHROMEOS)) &&                                   \
    BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#endif

// This file maps service classes to sandbox types. See
// ServiceProcessHost::Launch() for how these templates are consumed.

// printing::mojom::PrintBackendService
#if (defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
     defined(OS_CHROMEOS)) &&                                   \
    BUILDFLAG(ENABLE_PRINTING)
namespace printing {
namespace mojom {
class PrintBackendService;
}
}  // namespace printing

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<printing::mojom::PrintBackendService>() {
  return printing::PrintBackendServiceManager::GetInstance()
                 .ShouldSandboxPrintBackendService()
             ? sandbox::policy::SandboxType::kPrintBackend
             : sandbox::policy::SandboxType::kNoSandbox;
}
#endif  // (defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        //  defined(OS_CHROMEOS)) &&
        // BUILDFLAG(ENABLE_PRINTING)

#endif  // CHROME_BROWSER_SERVICE_SANDBOX_TYPE_H_
