// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/api/test_support/system_extensions_api_browsertest.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/system_extensions/api/test_support/system_extensions_test_runner.test-mojom.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"

namespace ash {

namespace internal {

TestChromeContentBrowserClient::TestChromeContentBrowserClient() = default;

TestChromeContentBrowserClient::~TestChromeContentBrowserClient() = default;

void TestChromeContentBrowserClient::Init() {
  scoped_content_browser_client_setting_ =
      std::make_unique<content::ScopedContentBrowserClientSetting>(this);
}

void TestChromeContentBrowserClient::BindHostReceiverForRenderer(
    content::RenderProcessHost* render_process_host,
    mojo::GenericPendingReceiver receiver) {
  // Copy the name here instead of using a const reference because `PassPipe()`
  // below will reset the interface name.
  const std::string interface_name = *receiver.interface_name();
  if (binder_registry_.CanBindInterface(interface_name)) {
    binder_registry_.BindInterface(interface_name, receiver.PassPipe());
    return;
  }

  ChromeContentBrowserClient::BindHostReceiverForRenderer(render_process_host,
                                                          std::move(receiver));
}

}  // namespace internal

namespace {

constexpr SystemExtensionId kTestSystemExtensionId = {1, 2, 3, 4};

// Class that receives events from the running test.
class TestRunner : public system_extensions_test::mojom::TestRunner {
 public:
  TestRunner() = default;

  ~TestRunner() override = default;

  // Returns once the test calls `OnCompletion` i.e. when the test finishes
  // running.
  testing::AssertionResult WaitForCompletion() {
    run_loop_.Run();

    if (!result_.has_value())
      return testing::AssertionFailure() << "Test timed out.";

    return result_.value();
  }

  void Bind(mojo::PendingReceiver<system_extensions_test::mojom::TestRunner>
                pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  // system_extensions_test::mojom::TestRunner
  void OnCompletion(
      std::vector<system_extensions_test::mojom::TestResultPtr> tests_results,
      system_extensions_test::mojom::TestHarnessResultPtr testharness_result)
      override {
    base::ScopedClosureRunner scoped_runner(run_loop_.QuitClosure());

    if (testharness_result->status !=
        system_extensions_test::mojom::TestHarnessStatus::kOk) {
      result_ = testing::AssertionFailure()
                << "Test harness failure.\n"
                << testharness_result->message.value() << "\n"
                << testharness_result->stack.value();
      return;
    }

    // We do this to keep this method simple. If we decide multiple tests per
    // file are needed, we can change this.
    if (tests_results.size() != 1u) {
      result_ = testing::AssertionFailure()
                << "Only one test per file is currently supported.";
      return;
    }

    const auto& test_result = tests_results[0];
    if (test_result->status !=
        system_extensions_test::mojom::TestStatus::kPass) {
      result_ = testing::AssertionFailure()
                << test_result->message.value() << "\n"
                << test_result->stack.value();
      return;
    }

    result_ = testing::AssertionSuccess();
  }

 private:
  mojo::Receiver<system_extensions_test::mojom::TestRunner> receiver_{this};

  absl::optional<testing::AssertionResult> result_;

  base::RunLoop run_loop_;
};

base::FilePath GetAbsolutePathFromSrcRelative(base::StringPiece dir) {
  base::FilePath src_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));
  return src_dir.AppendASCII(dir);
}

}  // namespace

SystemExtensionsApiBrowserTest::SystemExtensionsApiBrowserTest(const Args& args)
    : tests_dir_(GetAbsolutePathFromSrcRelative(args.tests_dir)),
      manifest_template_(args.manifest_template),
      additional_src_files_(args.additional_src_files),
      additional_gen_files_(args.additional_gen_files),
      test_chrome_content_browser_client_(
          std::make_unique<internal::TestChromeContentBrowserClient>()) {
  feature_list_.InitWithFeatures(
      {features::kSystemExtensions,
       ::features::kEnableServiceWorkersForChromeUntrusted},
      {});

  test_chrome_content_browser_client_->AddRendererInterface(
      base::BindLambdaForTesting(
          [this](
              mojo::PendingReceiver<system_extensions_test::mojom::TestRunner>
                  receiver) {
            this->test_runner_->Bind(std::move(receiver));
          }));
}

SystemExtensionsApiBrowserTest::~SystemExtensionsApiBrowserTest() = default;

void SystemExtensionsApiBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  test_runner_ = std::make_unique<TestRunner>();
  test_chrome_content_browser_client_->Init();
}

void SystemExtensionsApiBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  InProcessBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitchASCII(::switches::kEnableBlinkFeatures, "MojoJS");
}

void SystemExtensionsApiBrowserTest::RunTest(base::StringPiece test_file_name) {
  EXPECT_PRED_FORMAT1(RunTestImpl, test_file_name);
}

testing::AssertionResult SystemExtensionsApiBrowserTest::RunTestImpl(
    base::StringPiece test_file_name_expr,
    base::StringPiece test_file_name) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::ScopedTempDir system_extension_dir;
  CHECK(system_extension_dir.CreateUniqueTempDir());
  const base::FilePath system_extension_path = system_extension_dir.GetPath();

  // Copy test resources.
  base::FilePath web_test_resources_path =
      GetAbsolutePathFromSrcRelative("third_party/blink/web_tests/resources");

  static constexpr const char* web_test_resources[] = {
      "testharness.js",
      "testharness-helpers.js",
  };

  for (base::StringPiece resource : base::make_span(web_test_resources)) {
    CHECK(base::CopyFile(web_test_resources_path.AppendASCII(resource),
                         system_extension_path.AppendASCII(resource)))
        << "Failed to copy Web Tests resources: " << resource;
  }

  // Copy test support files.
  const base::FilePath test_support_dir = GetAbsolutePathFromSrcRelative(
      "chrome/browser/ash/system_extensions/api/test_support/");
  static constexpr const char* test_support_resources[] = {
      "testharnessreport.js",
      "test_support.js",
  };

  for (base::StringPiece resource : base::make_span(test_support_resources)) {
    CHECK(base::CopyFile(test_support_dir.AppendASCII(resource),
                         system_extension_path.AppendASCII(resource)))
        << "Failed to copy test support resource: " << resource;
  }

  // Copy required test runner Mojo files.
  base::FilePath gen_dir;
  CHECK(base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &gen_dir));

  struct MojomFiles {
    const char* path;
    const char* name;
  } mojom_files[] = {
      {"gen/mojo/public/js/", "mojo_bindings_lite.js"},
      {"gen/chrome/browser/ash/system_extensions/api/test_support/",
       "system_extensions_test_runner.test-mojom-lite.js"},
  };
  for (auto mojom_file : base::make_span(mojom_files)) {
    CHECK(base::CopyFile(
        gen_dir.AppendASCII(mojom_file.path).AppendASCII(mojom_file.name),
        system_extension_path.AppendASCII(mojom_file.name)))
        << "Failed to copy mojo resource: " << mojom_file.name;
  }

  // Copy additional files in the output directory.
  for (const std::string& file_path : additional_gen_files_) {
    base::FilePath absolute_path = gen_dir.AppendASCII(file_path);
    CHECK(base::CopyFile(
        absolute_path, system_extension_path.Append(absolute_path.BaseName())))
        << "Failed to copy additional gen file: " << absolute_path.BaseName();
  }

  // Copy additional files.
  for (const std::string& file_path : additional_src_files_) {
    base::FilePath absolute_path = GetAbsolutePathFromSrcRelative(file_path);
    CHECK(base::CopyFile(
        absolute_path, system_extension_path.Append(absolute_path.BaseName())))
        << "Failed to copy additional file: " << absolute_path.BaseName();
  }

  // Write manifest.
  const std::string manifest_str =
      base::StringPrintf(manifest_template_.data(), test_file_name.data());
  CHECK(base::WriteFile(system_extension_path.AppendASCII("manifest.json"),
                        manifest_str))
      << "Failed to write the manifest.";

  // Copy test file.
  CHECK(base::CopyFile(tests_dir_.AppendASCII(test_file_name),
                       system_extension_path.AppendASCII(test_file_name)))
      << "Failed to copy the test file.";

  // Install a System Extension.
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  base::RunLoop run_loop;
  InstallStatusOrSystemExtensionId result;
  install_manager.InstallUnpackedExtensionFromDir(
      system_extension_path,
      base::BindLambdaForTesting([&](InstallStatusOrSystemExtensionId result) {
        ASSERT_TRUE(result.ok()) << "Failed to install System Extension: "
                                 << static_cast<int32_t>(result.status());
        ASSERT_EQ(kTestSystemExtensionId, result.value());
        run_loop.Quit();
      }));
  run_loop.Run();

  return test_runner_->WaitForCompletion();
}

}  // namespace ash
