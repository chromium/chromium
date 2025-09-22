// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_desktop.h"

#include "chrome/browser/apps/app_service/publisher_host.h"
#include "chrome/browser/apps/app_service/publisher_host_factory.h"

namespace apps {

AppServiceProxy::AppServiceProxy(Profile* profile,
                                 PublisherHostFactory* publisher_host_factory)
    : AppServiceProxyBase(profile, publisher_host_factory) {}

AppServiceProxy::~AppServiceProxy() = default;

void AppServiceProxy::Initialize() {
  if (!IsValidProfile()) {
    return;
  }

  AppServiceProxyBase::Initialize();
  publisher_host_ = publisher_host_factory_->CreatePublisherHost(this);
}

bool AppServiceProxy::MaybeShowLaunchPreventionDialog(
    const apps::AppUpdate& update) {
  return false;
}

}  // namespace apps
