// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/shortcut_publisher.h"

#include "base/check.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"

namespace apps {

ShortcutPublisher::ShortcutPublisher(AppServiceProxy* proxy) : proxy_(proxy) {
  CHECK(proxy);
}

ShortcutPublisher::~ShortcutPublisher() = default;

void ShortcutPublisher::RegisterShortcutPublisher(AppType app_type) {
  proxy_->RegisterShortcutPublisher(app_type, this);
}

void ShortcutPublisher::PublishShortcut(ShortcutPtr delta) {
  CHECK(proxy_->ShortcutRegistryCache());
  proxy_->ShortcutRegistryCache()->UpdateShortcut(std::move(delta));
}

void ShortcutPublisher::ShortcutRemoved(const ShortcutId& id) {
  CHECK(proxy_->ShortcutRegistryCache());
  proxy_->ShortcutRegistryCache()->RemoveShortcut(id);
}

}  // namespace apps
