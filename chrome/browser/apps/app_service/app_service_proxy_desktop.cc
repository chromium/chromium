// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_desktop.h"

#include "chrome/browser/apps/app_service/publisher_host.h"

namespace apps {

AppServiceProxy::AppServiceProxy(Profile* profile)
    : AppServiceProxyBase(profile) {}

AppServiceProxy::~AppServiceProxy() = default;

void AppServiceProxy::Initialize() {
  if (!IsValidProfile()) {
    return;
  }

  AppServiceProxyBase::Initialize();

  publisher_host_ = std::make_unique<PublisherHost>(this);
}

bool AppServiceProxy::MaybeShowLaunchPreventionDialog(
    const apps::AppUpdate& update) {
  return false;
}

}  // namespace apps
