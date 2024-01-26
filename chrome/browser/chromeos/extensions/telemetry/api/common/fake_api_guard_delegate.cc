// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/fake_api_guard_delegate.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
}

namespace chromeos {

FakeApiGuardDelegate::Factory::Factory(std::optional<std::string> error_message)
    : error_message_(error_message) {}

FakeApiGuardDelegate::Factory::~Factory() = default;

std::unique_ptr<ApiGuardDelegate>
FakeApiGuardDelegate::Factory::CreateInstance() {
  return base::WrapUnique<ApiGuardDelegate>(
      new FakeApiGuardDelegate(error_message_));
}

FakeApiGuardDelegate::FakeApiGuardDelegate(
    std::optional<std::string> error_message)
    : error_message_(error_message) {}

FakeApiGuardDelegate::~FakeApiGuardDelegate() = default;

void FakeApiGuardDelegate::CanAccessApi(content::BrowserContext* context,
                                        const extensions::Extension* extension,
                                        CanAccessApiCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), error_message_));
}

}  // namespace chromeos
