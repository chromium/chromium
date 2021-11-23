// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command_factory.h"

#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_WIN)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/win_key_rotation_command.h"
#endif  // defined(OS_WIN)

namespace enterprise_connectors {

namespace {

absl::optional<KeyRotationCommandFactory*>& GetTestInstanceStorage() {
  static absl::optional<KeyRotationCommandFactory*> storage;
  return storage;
}

}  // namespace

KeyRotationCommandFactory::~KeyRotationCommandFactory() = default;

// static
KeyRotationCommandFactory* KeyRotationCommandFactory::GetInstance() {
  auto test_instance = GetTestInstanceStorage();
  if (test_instance.has_value() && test_instance.value()) {
    return test_instance.value();
  }
  return base::Singleton<KeyRotationCommandFactory>::get();
}

std::unique_ptr<KeyRotationCommand> KeyRotationCommandFactory::CreateCommand() {
#if defined(OS_WIN)
  return std::make_unique<WinKeyRotationCommand>();
#else
  return nullptr;
#endif  // defined(OS_WIN)
}

// static
void KeyRotationCommandFactory::SetFactoryInstanceForTesting(
    KeyRotationCommandFactory* factory) {
  DCHECK(factory);
  GetTestInstanceStorage().emplace(factory);
}

// static
void KeyRotationCommandFactory::ClearFactoryInstanceForTesting() {
  GetTestInstanceStorage().reset();
}

}  // namespace enterprise_connectors
