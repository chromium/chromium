// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/services_delegate.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/telemetry/telemetry_service.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_network_context.h"
#include "components/safe_browsing/core/browser/db/v4_local_database_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

namespace safe_browsing {

ServicesDelegate::ServicesDelegate(
    SafeBrowsingServiceImpl* safe_browsing_service,
    ServicesCreator* services_creator)
    : safe_browsing_service_(safe_browsing_service),
      services_creator_(services_creator) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ServicesDelegate::~ServicesDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ServicesDelegate::ShutdownServices() {
}

}  // namespace safe_browsing
