// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>

#include "ash/webui/demo_mode_app_ui/demo_mode_app_untrusted_ui.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace ash {
namespace {

const std::string kFileContents = "Test File Contents";

class DemoModeAppUntrustedUITest : public testing::Test {
 protected:
  DemoModeAppUntrustedUITest() = default;
  ~DemoModeAppUntrustedUITest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::File file = base::CreateAndOpenTemporaryFileInDir(
        temp_dir_.GetPath(), &content_file_path_);
    base::WriteFile(content_file_path_, kFileContents);

    scheme_registry_ = std::make_unique<url::ScopedSchemeRegistryForTests>();
    url::AddStandardScheme("chrome-untrusted", url::SCHEME_WITH_HOST);
  }

  base::FilePath content_file_path_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<url::ScopedSchemeRegistryForTests> scheme_registry_;
  base::test::TaskEnvironment task_environment_;
};

void VerifyDataResponse(std::string expected_response,
                        base::OnceClosure quit_closure,
                        scoped_refptr<base::RefCountedMemory> data_response) {
  EXPECT_EQ(base::as_string_view(*data_response), expected_response);
  std::move(quit_closure).Run();
}

TEST_F(DemoModeAppUntrustedUITest, SourceDataFromComponent) {
  base::RunLoop run_loop;
  DemoModeAppUntrustedUI::SourceDataFromComponent(
      temp_dir_.GetPath(), content_file_path_.BaseName().MaybeAsASCII(),
      base::BindOnce(&VerifyDataResponse, kFileContents,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(DemoModeAppUntrustedUITest, SourceDataFromComponentQueryParam) {
  base::RunLoop run_loop;
  std::string resource_path_with_query_param =
      content_file_path_.BaseName().MaybeAsASCII() + "?testparam=testvalue";

  DemoModeAppUntrustedUI::SourceDataFromComponent(
      temp_dir_.GetPath(), resource_path_with_query_param,
      base::BindOnce(&VerifyDataResponse, kFileContents,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(DemoModeAppUntrustedUITest, SourceDataFromComponentURLFragment) {
  base::RunLoop run_loop;
  std::string resource_path_with_url_fragment =
      content_file_path_.BaseName().MaybeAsASCII() + "#frag";

  DemoModeAppUntrustedUI::SourceDataFromComponent(
      temp_dir_.GetPath(), resource_path_with_url_fragment,
      base::BindOnce(&VerifyDataResponse, kFileContents,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(DemoModeAppUntrustedUITest,
       SourceDataFromComponentQueryParamAndURLFragment) {
  base::RunLoop run_loop;
  std::string resource_path_with_url_fragment =
      content_file_path_.BaseName().MaybeAsASCII() +
      "?testparam=testvalue#frag";

  DemoModeAppUntrustedUI::SourceDataFromComponent(
      temp_dir_.GetPath(), resource_path_with_url_fragment,
      base::BindOnce(&VerifyDataResponse, kFileContents,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(DemoModeAppUntrustedUITest, SourceDataFromComponentParentDirReference) {
  base::RunLoop run_loop;
  // Treat temp_dir_ as the parent of the component directory here, that
  // a malicious ".."-containing path may be trying to access
  base::ScopedTempDir component_dir;
  ASSERT_TRUE(component_dir.CreateUniqueTempDirUnderPath(temp_dir_.GetPath()));
  std::string resource_path_with_parent_ref =
      "../" + content_file_path_.BaseName().MaybeAsASCII();

  DemoModeAppUntrustedUI::SourceDataFromComponent(
      component_dir.GetPath(), resource_path_with_parent_ref,
      base::BindOnce(&VerifyDataResponse, "", run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace
}  // namespace ash
