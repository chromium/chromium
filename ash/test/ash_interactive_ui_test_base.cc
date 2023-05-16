// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_interactive_ui_test_base.h"

#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace ash {

namespace {

class MojoInitializer {
 public:
  MojoInitializer() { mojo::core::Init(); }
};

base::LazyInstance<MojoInitializer>::Leaky mojo_initializer;

// Initialize mojo once per process.
void InitializeMojo() {
  mojo_initializer.Get();
}

}  // namespace

AshInteractiveUITestBase::AshInteractiveUITestBase() = default;

AshInteractiveUITestBase::~AshInteractiveUITestBase() = default;

void AshInteractiveUITestBase::SetUp() {
  InitializeMojo();

  gl::GLSurfaceTestSupport::InitializeOneOff();

  ui::RegisterPathProvider();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  base::FilePath resources_pack_path;
  base::PathService::Get(base::DIR_MODULE, &resources_pack_path);
  resources_pack_path =
      resources_pack_path.Append(FILE_PATH_LITERAL("resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::kScaleFactorNone);
  env_ = aura::Env::CreateInstance();

  AshTestBase::SetUp();
}

void AshInteractiveUITestBase::TearDown() {
  AshTestBase::TearDown();
  env_.reset();
}

}  // namespace ash
