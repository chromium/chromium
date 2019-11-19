// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service.h"

#include <string.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browser_switcher {

namespace {

// A URL that should trigger a switch.
const char kTestUrl[] = "http://example.com/foobar";
const char kTestUrlWithSpaces[] = "http://example.com/foobar baz";

// A URL that shouldn't trigger a switch.
const char kOtherUrl[] = "http://google.com/";

// |echo| adds a newline at the end of the file. CRLF on Windows, but just LF on
// POSIX systems.
#if defined(OS_WIN)
const char kTestUrlWithLineEnding[] = "http://example.com/foobar\r\n";
#else
const char kTestUrlWithLineEnding[] = "http://example.com/foobar\n";
#endif

#if defined(OS_WIN)
std::string NativeToUTF8(const std::wstring& native) {
  return base::UTF16ToUTF8(native);
}
#else
std::string NativeToUTF8(const std::string& native) {
  return native;
}
#endif

void SetPolicy(policy::PolicyMap* map,
               const std::string& policy_name,
               std::unique_ptr<base::Value> value) {
  map->Set(policy_name, policy::POLICY_LEVEL_MANDATORY,
           policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
           std::move(value), nullptr);
}

// Sets the following policies, in the Chrome policy domain:
// - AlternativeBrowserPath: cmd_line's command
// - AlternativeBrowserParameters: cmd_line's parameters
// - BrowserSwitcherUrlList: ["example.com"]
void InitPolicies(policy::MockConfigurationPolicyProvider* provider,
                  const base::CommandLine& cmd_line) {
  policy::PolicyMap map;

  SetPolicy(&map, policy::key::kBrowserSwitcherEnabled,
            std::make_unique<base::Value>(true));
  SetPolicy(
      &map, policy::key::kAlternativeBrowserPath,
      std::make_unique<base::Value>(cmd_line.GetProgram().MaybeAsASCII()));

  base::ListValue params;
  for (size_t i = 1; i < cmd_line.argv().size(); i++)
    params.Append(NativeToUTF8(cmd_line.argv()[i]));
  SetPolicy(&map, policy::key::kAlternativeBrowserParameters,
            std::make_unique<base::Value>(std::move(params)));

  base::ListValue sitelist;
  sitelist.Append("example.com");
  SetPolicy(&map, policy::key::kBrowserSwitcherUrlList,
            std::make_unique<base::Value>(std::move(sitelist)));

  provider->UpdateChromePolicy(map);
  base::RunLoop().RunUntilIdle();
}

// Generates a command-line that prints the URL to a file when run. Make sure
// the navigation URL and |output_file| don't contain any special characters or
// whitespace.
base::CommandLine GenerateEchoCommandLine(const base::FilePath& output_file) {
#if defined(OS_WIN)
  // cmd.exe /C echo ${url} > "output_file"
  std::vector<std::wstring> args = {
      L"cmd.exe",
      base::UTF8ToUTF16(base::StringPrintf("/C echo ${url}> \"%s\"",
                                           output_file.MaybeAsASCII().c_str())),
  };
  return base::CommandLine(std::move(args));
#else
  // bin/sh -c 'echo "${url}"> "output_file"'
  std::vector<std::string> args = {
      "/bin/sh", "-c",
      base::StringPrintf("echo \"${url}\" > \"%s\"",
                         output_file.MaybeAsASCII().c_str()),
  };
  return base::CommandLine(std::move(args));
#endif
}

}  // namespace

class BrowserSwitcherBrowserTest : public InProcessBrowserTest {
 public:
  BrowserSwitcherBrowserTest() = default;
  ~BrowserSwitcherBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  policy::MockConfigurationPolicyProvider* provider() { return &provider_; }
  base::FilePath GetTempDir() { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
  policy::MockConfigurationPolicyProvider provider_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSwitcherBrowserTest);
};

IN_PROC_BROWSER_TEST_F(BrowserSwitcherBrowserTest, RunsExternalCommand) {
  base::FilePath temp_file =
      GetTempDir().AppendASCII("RunsExternalCommand.txt");
  base::CommandLine cmd_line = GenerateEchoCommandLine(temp_file);

  InitPolicies(provider(), cmd_line);

  // We open a new tab, because closing the last tab in the browser
  // causes the whole browser to close.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(kTestUrl), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath path, base::OnceClosure quit) {
            base::ScopedAllowBlockingForTesting allow_blocking;
            base::File file(path,
                            base::File::FLAG_OPEN | base::File::FLAG_READ);
            ASSERT_TRUE(file.IsValid());
            EXPECT_EQ(static_cast<int64_t>(strlen(kTestUrlWithLineEnding)),
                      file.GetLength());

            // File content should be equal to the navigated URL.
            std::unique_ptr<char[]> buffer(new char[file.GetLength() + 1]);
            buffer.get()[file.GetLength()] = '\0';
            file.Read(0, buffer.get(), file.GetLength());
            EXPECT_EQ(std::string(kTestUrlWithLineEnding),
                      std::string(buffer.get()));

            std::move(quit).Run();
          },
          std::move(temp_file), run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherBrowserTest, DoesNotKeepSpaces) {
  base::FilePath temp_file =
      GetTempDir().AppendASCII("RunsExternalCommand.txt");
  base::CommandLine cmd_line = GenerateEchoCommandLine(temp_file);

  InitPolicies(provider(), cmd_line);

  // We open a new tab, because closing the last tab in the browser
  // causes the whole browser to close.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(kTestUrlWithSpaces),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath path, base::OnceClosure quit) {
            base::ScopedAllowBlockingForTesting allow_blocking;
            base::File file(path,
                            base::File::FLAG_OPEN | base::File::FLAG_READ);
            ASSERT_TRUE(file.IsValid());

            std::unique_ptr<char[]> buffer(new char[file.GetLength() + 1]);
            buffer.get()[file.GetLength()] = '\0';
            file.Read(0, buffer.get(), file.GetLength());
            // Check that there's no space in the URL (i.e. replaced with %20).
            EXPECT_FALSE(strchr(buffer.get(), ' '));
            EXPECT_TRUE(strstr(buffer.get(), "%20"));

            std::move(quit).Run();
          },
          std::move(temp_file), run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherBrowserTest, DoesNotRunOnRandomUrls) {
  base::FilePath temp_file =
      GetTempDir().AppendASCII("DoesNotRunOnRandomUrls.txt");
  base::CommandLine cmd_line = GenerateEchoCommandLine(temp_file);

  InitPolicies(provider(), cmd_line);

  // We open a new tab, because closing the last tab in the browser
  // causes the whole browser to close.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(kOtherUrl), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath path, base::OnceClosure quit) {
            base::ScopedAllowBlockingForTesting allow_blocking;
            EXPECT_FALSE(base::PathExists(path));
            std::move(quit).Run();
          },
          std::move(temp_file), run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}

}  // namespace browser_switcher
