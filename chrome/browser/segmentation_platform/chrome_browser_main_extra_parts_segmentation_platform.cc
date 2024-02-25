// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/chrome_browser_main_extra_parts_segmentation_platform.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/segmentation_platform/ukm_database_client.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

void ChromeBrowserMainExtraPartsSegmentationPlatform::PreCreateThreads() {
  segmentation_platform::LocalStateHelper::GetInstance().Initialize(
      g_browser_process->local_state());
}

void ChromeBrowserMainExtraPartsSegmentationPlatform::PreProfileInit() {
  segmentation_platform::UkmDatabaseClientHolder::GetClientInstance(nullptr)
      .PreProfileInit(
          /*in_memory_database=*/false);
}

void ChromeBrowserMainExtraPartsSegmentationPlatform::PostProfileInit(
    Profile* profile,
    bool is_initial_profile) {
  if (!profile || profile->IsOffTheRecord())
    return;

  // Always create SegmentationPlatformService when a new Profile is
  // initialized. This will trigger model downloads and feature storage, so when
  // the client requests segment the platform is ready with results.
  segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
      profile);
}

void ChromeBrowserMainExtraPartsSegmentationPlatform::PostMainMessageLoopRun() {
  segmentation_platform::UkmDatabaseClientHolder::GetClientInstance(nullptr)
      .PostMessageLoopRun();
}
