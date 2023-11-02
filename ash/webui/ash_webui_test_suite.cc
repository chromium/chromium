// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/ash_webui_test_suite.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace {

// Initializes the i18n stack and loads the necessary strings. Uses a specific
// locale, so that the tests can compare against golden strings without
// depending on the environment.
void InitI18n() {
  base::i18n::SetICUDefaultLocale("en_US");

  ui::RegisterPathProvider();

  base::FilePath ui_test_pak_path;
  ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

  base::FilePath dir_assets_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &dir_assets_path));
  base::FilePath chromeos_test_strings_path =
      dir_assets_path.Append(FILE_PATH_LITERAL("chromeos_test_strings.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      chromeos_test_strings_path, ui::kScaleFactorNone);
}

}  // namespace

AshWebUITestSuite::AshWebUITestSuite(int argc, char** argv)
    : TestSuite(argc, argv) {}

AshWebUITestSuite::~AshWebUITestSuite() = default;

void AshWebUITestSuite::Initialize() {
  base::TestSuite::Initialize();

  gl::GLSurfaceTestSupport::InitializeOneOff();

  InitI18n();
}

void AshWebUITestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}
