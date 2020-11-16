// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SERVICE_SANDBOX_TYPE_H_
#define CHROME_BROWSER_CHROMEOS_SERVICE_SANDBOX_TYPE_H_

#include "content/public/browser/service_process_host.h"
#include "sandbox/policy/sandbox_type.h"

// This file maps service classes to sandbox types.  Services which
// require a non-utility sandbox can be added here.  See
// ServiceProcessHost::Launch() for how these templates are consumed.

// chromeos::ime::mojom::ImeService
namespace chromeos {
namespace ime {
namespace mojom {
class ImeService;
}  // namespace mojom
}  // namespace ime
}  // namespace chromeos

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<chromeos::ime::mojom::ImeService>() {
  return sandbox::policy::SandboxType::kIme;
}

// chromeos::tts::mojom::TtsService
namespace chromeos {
namespace tts {
namespace mojom {
class TtsService;
}  // namespace mojom
}  // namespace tts
}  // namespace chromeos

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<chromeos::tts::mojom::TtsService>() {
  return sandbox::policy::SandboxType::kTts;
}

#endif  // CHROME_BROWSER_CHROMEOS_SERVICE_SANDBOX_TYPE_H_
