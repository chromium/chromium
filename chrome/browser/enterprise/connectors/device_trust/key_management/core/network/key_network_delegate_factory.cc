// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate_factory.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"

#if defined(OS_WIN)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/win_key_network_delegate.h"
#endif

namespace enterprise_connectors {

std::unique_ptr<KeyNetworkDelegate> CreateKeyNetworkDelegate() {
#if defined(OS_WIN)
  return std::make_unique<WinKeyNetworkDelegate>();
#else
  NOTREACHED();
  return nullptr;
#endif  // defined(OS_WIN)
}

}  // namespace enterprise_connectors
