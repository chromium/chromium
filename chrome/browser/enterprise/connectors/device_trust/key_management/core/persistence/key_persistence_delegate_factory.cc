// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/win_key_persistence_delegate.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mac_key_persistence_delegate.h"
#elif BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/linux_key_persistence_delegate.h"
#endif

namespace enterprise_connectors {

namespace {

std::optional<KeyPersistenceDelegateFactory*>& GetTestInstanceStorage() {
  static std::optional<KeyPersistenceDelegateFactory*> storage;
  return storage;
}

}  // namespace

// static
KeyPersistenceDelegateFactory* KeyPersistenceDelegateFactory::GetInstance() {
  std::optional<KeyPersistenceDelegateFactory*>& test_instance =
      GetTestInstanceStorage();
  if (test_instance.has_value() && test_instance.value()) {
    return test_instance.value();
  }
  static base::NoDestructor<KeyPersistenceDelegateFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyPersistenceDelegate>
KeyPersistenceDelegateFactory::CreateKeyPersistenceDelegate() {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<WinKeyPersistenceDelegate>();
#elif BUILDFLAG(IS_MAC)
  return std::make_unique<MacKeyPersistenceDelegate>();
#elif BUILDFLAG(IS_LINUX)
  return std::make_unique<LinuxKeyPersistenceDelegate>();
#else
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#endif
}

// static
void KeyPersistenceDelegateFactory::SetInstanceForTesting(
    KeyPersistenceDelegateFactory* factory) {
  DCHECK(factory);
  GetTestInstanceStorage().emplace(factory);
}

// static
void KeyPersistenceDelegateFactory::ClearInstanceForTesting() {
  GetTestInstanceStorage().reset();
}

}  // namespace enterprise_connectors
