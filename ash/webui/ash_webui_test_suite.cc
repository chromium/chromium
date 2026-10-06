// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/ash_webui_test_suite.h"

#include "ash/constants/ash_paths.h"
#include "base/path_service.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

AshWebUITestSuite::AshWebUITestSuite(int argc, char** argv)
    : TestSuite(argc, argv) {}

AshWebUITestSuite::~AshWebUITestSuite() = default;

void AshWebUITestSuite::Initialize() {
  base::TestSuite::Initialize();

  gl::GLSurfaceTestSupport::InitializeOneOff();

  ui::RegisterPathProvider();
  ash::AshTestSuite::LoadTestResources();

  ash::RegisterPathProvider();
  CHECK(user_data_dir_.CreateUniqueTempDir());
  CHECK(base::PathService::OverrideAndCreateIfNeeded(
      ash::DIR_USER_DATA, user_data_dir_.GetPath(),
      /*is_absolute=*/true, /*create=*/false));

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
}

void AshWebUITestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}
