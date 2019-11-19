// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/test/ar_test_suite.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace vr {

ArTestSuite::ArTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

ArTestSuite::~ArTestSuite() = default;

void ArTestSuite::Initialize() {
  base::TestSuite::Initialize();

  task_environment_ = std::make_unique<base::test::TaskEnvironment>(
      base::test::TaskEnvironment::MainThreadType::UI);

  mojo::core::Init();

  base::FilePath pak_path;
  ui::RegisterPathProvider();
  base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      pak_path.AppendASCII("vr_test.pak"));
  ui::MaterialDesignController::Initialize();
}

void ArTestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace vr
