// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/storage_notification_service_impl.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"

#if !defined(OS_ANDROID)
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/notification.h"
#endif

base::RepeatingClosure
StorageNotificationServiceImpl::GetStoragePressureNotificationClosure() {
  return base::BindRepeating([](Profile* profile) {}, profile_);
}

StorageNotificationServiceImpl::StorageNotificationServiceImpl() {}

StorageNotificationServiceImpl::~StorageNotificationServiceImpl() = default;
