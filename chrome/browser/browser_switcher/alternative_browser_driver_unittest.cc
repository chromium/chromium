// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/alternative_browser_driver.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browser_switcher {

using StringType = base::FilePath::StringType;

namespace {

class TestBrowserSwitcherPrefs : public BrowserSwitcherPrefs {
 public:
  explicit TestBrowserSwitcherPrefs(PrefService* prefs)
      : BrowserSwitcherPrefs(prefs, nullptr) {}
};

StringType UTF8ToNative(std::string_view src) {
#if BUILDFLAG(IS_WIN)
  return base::UTF8ToWide(src);
#elif BUILDFLAG(IS_POSIX)
  return std::string(src);
#else
#error "Invalid platform for browser_switcher"
#endif
}

base::Value::List UTF8VectorToValueList(
    const std::vector<std::string_view>& src) {
  base::Value::List out;
  for (std::string_view str : src) {
    out.Append(str);
  }
  return out;
}

}  // namespace

class AlternativeBrowserDriverTest : public testing::Test {
 public:
  void SetUp() override {
    BrowserSwitcherPrefs::RegisterProfilePrefs(prefs_backend_.registry());
    prefs_ = std::make_unique<TestBrowserSwitcherPrefs>(&prefs_backend_);
    driver_ = std::make_unique<AlternativeBrowserDriverImpl>(prefs_.get());
  }

  void SetBrowserPath(const std::string& path) {
    prefs_backend_.SetManagedPref(prefs::kAlternativeBrowserPath,
                                  base::Value(path));
  }

  void SetBrowserParameters(const base::Value::List& params) {
    prefs_backend_.SetManagedPref(prefs::kAlternativeBrowserParameters,
                                  base::Value(params.Clone()));
  }

  AlternativeBrowserDriverImpl* driver() { return driver_.get(); }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_backend_;
  std::unique_ptr<BrowserSwitcherPrefs> prefs_;
  std::unique_ptr<AlternativeBrowserDriverImpl> driver_;
};

TEST_F(AlternativeBrowserDriverTest, CreateCommandLine) {
  SetBrowserPath("/usr/bin/true");
  auto params = UTF8VectorToValueList({"a", "b", "c"});
  SetBrowserParameters(params);
  auto cmd_line = driver()->CreateCommandLine(GURL("http://example.com/"));
  const base::CommandLine::StringVector& argv = cmd_line.argv();
  EXPECT_EQ(5u, argv.size());
  EXPECT_EQ(UTF8ToNative("/usr/bin/true"), argv[0]);
  EXPECT_EQ(UTF8ToNative("a"), argv[1]);
  EXPECT_EQ(UTF8ToNative("b"), argv[2]);
  EXPECT_EQ(UTF8ToNative("c"), argv[3]);
  EXPECT_EQ(UTF8ToNative("http://example.com/"), argv[4]);
}

TEST_F(AlternativeBrowserDriverTest, CreateCommandLineExpandsUrl) {
  SetBrowserPath("/usr/bin/true");
  auto params = UTF8VectorToValueList({"--flag=${url}#fragment"});
  SetBrowserParameters(params);
  auto cmd_line = driver()->CreateCommandLine(GURL("http://example.com/"));
  const base::CommandLine::StringVector& argv = cmd_line.argv();
  EXPECT_EQ(2u, argv.size());
  EXPECT_EQ(UTF8ToNative("/usr/bin/true"), argv[0]);
  EXPECT_EQ(UTF8ToNative("--flag=http://example.com/#fragment"), argv[1]);
}

TEST_F(AlternativeBrowserDriverTest, GetBrowserName) {
#if BUILDFLAG(IS_WIN)
  std::string expected = "Internet Explorer";
#elif BUILDFLAG(IS_MAC)
  std::string expected = "Safari";
#else
  std::string expected;
#endif
  std::string actual = driver()->GetBrowserName();
  EXPECT_EQ(expected, actual);

  SetBrowserPath("bogus.exe");
  actual = driver()->GetBrowserName();
  EXPECT_EQ("", actual);

#if BUILDFLAG(IS_WIN)
  SetBrowserPath("${ie}");
  actual = driver()->GetBrowserName();
  EXPECT_EQ("Internet Explorer", actual);

#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  SetBrowserPath("${safari}");
  actual = driver()->GetBrowserName();
  EXPECT_EQ("Safari", actual);

  SetBrowserPath("${edge}");
  actual = driver()->GetBrowserName();
  EXPECT_EQ("Microsoft Edge", actual);
#endif

  SetBrowserPath("${firefox}");
  actual = driver()->GetBrowserName();
  EXPECT_EQ("Mozilla Firefox", actual);

  SetBrowserPath("${opera}");
  actual = driver()->GetBrowserName();
  EXPECT_EQ("Opera", actual);
}

TEST_F(AlternativeBrowserDriverTest, GetBrowserType) {
#if BUILDFLAG(IS_WIN)
  BrowserType expected = BrowserType::kIE;
#elif BUILDFLAG(IS_MAC)
  BrowserType expected = BrowserType::kSafari;
#else
  BrowserType expected = BrowserType::kUnknown;
#endif
  auto actual = driver()->GetBrowserType();
  EXPECT_EQ(expected, actual);

  SetBrowserPath("bogus.exe");
  actual = driver()->GetBrowserType();
  EXPECT_EQ(BrowserType::kUnknown, actual);

#if BUILDFLAG(IS_WIN)
  SetBrowserPath("${ie}");
  actual = driver()->GetBrowserType();
  EXPECT_EQ(BrowserType::kIE, actual);

  SetBrowserPath("C:\\Program Files (x86)\\Internet Explorer\\IExplore.exe");
  actual = driver()->GetBrowserType();
  EXPECT_EQ(BrowserType::kIE, actual);

  SetBrowserPath("C:\\Program Files\\Mozilla Firefox\\firefox.exe");
  actual = driver()->GetBrowserType();
  EXPECT_EQ(BrowserType::kFirefox, actual);

  SetBrowserPath(
      "C:\\users\\HongGilDong\\AppData\\Local\\Programs\\Opera\\launcher.exe");
  actual = driver()->GetBrowserType();
  EXPECT_EQ(BrowserType::kOpera, actual);

  SetBrowserPath(
      "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe");
  actual = driver()->GetBrowserType();
  EXPECT_EQ(BrowserType::kChrome, actual);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  SetBrowserPath("${safari}");
  actual = driver()->GetBrowserType();
  EXPECT_EQ(BrowserType::kSafari, actual);

  SetBrowserPath("${edge}");
  actual = driver()->GetBrowserType();
  EXPECT_EQ(BrowserType::kEdge, actual);
#endif

  SetBrowserPath("${firefox}");
  actual = driver()->GetBrowserType();
  EXPECT_EQ(BrowserType::kFirefox, actual);

  SetBrowserPath("${opera}");
  actual = driver()->GetBrowserType();
  EXPECT_EQ(BrowserType::kOpera, actual);
}

#if BUILDFLAG(IS_WIN)
TEST_F(AlternativeBrowserDriverTest, CreateCommandLineExpandsEnvVars) {
  _putenv("A=AAA");
  _putenv("B=BBB");
  _putenv("CC=CCC");
  _putenv("D=DDD");
  SetBrowserPath("something.exe");
  auto params = UTF8VectorToValueList(
      {"%A%", "%B%", "before_%CC%_between_%D%_after", "%NONEXISTENT%"});
  SetBrowserParameters(params);
  auto cmd_line = driver()->CreateCommandLine(GURL("http://example.com/"));
  const base::CommandLine::StringVector& argv = cmd_line.argv();
  EXPECT_EQ(6u, argv.size());
  EXPECT_EQ(L"something.exe", argv[0]);
  EXPECT_EQ(L"AAA", argv[1]);
  EXPECT_EQ(L"BBB", argv[2]);
  EXPECT_EQ(L"before_CCC_between_DDD_after", argv[3]);
  EXPECT_EQ(L"%NONEXISTENT%", argv[4]);
  EXPECT_EQ(L"http://example.com/", argv[5]);
}

TEST_F(AlternativeBrowserDriverTest,
       AppendCommandLineArgumentDoesntExpandUrlContent) {
  _putenv("A=AAA");
  SetBrowserPath("something.exe");
  auto cmd_line = driver()->CreateCommandLine(GURL("http://evil.com/%A%"));
  EXPECT_EQ(2u, cmd_line.argv().size());
  EXPECT_EQ(L"something.exe", cmd_line.argv()[0]);
  EXPECT_EQ(L"http://evil.com/%A%", cmd_line.argv()[1]);

  auto params = UTF8VectorToValueList({"${url}"});
  SetBrowserParameters(params);
  cmd_line = driver()->CreateCommandLine(GURL("http://evil.com/%A%"));
  EXPECT_EQ(2u, cmd_line.argv().size());
  EXPECT_EQ(L"something.exe", cmd_line.argv()[0]);
  EXPECT_EQ(L"http://evil.com/%A%", cmd_line.argv()[1]);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
TEST_F(AlternativeBrowserDriverTest, CreateCommandLineUsesOpen) {
  // Use `open(1)' to launch browser paths that aren't absolute.

  // Default to launching Safari.
  SetBrowserPath("");
  auto cmd_line = driver()->CreateCommandLine(GURL("http://example.com/"));
  EXPECT_EQ(4u, cmd_line.argv().size());
  EXPECT_EQ("open", cmd_line.argv()[0]);
  EXPECT_EQ("-a", cmd_line.argv()[1]);
  EXPECT_EQ("Safari", cmd_line.argv()[2]);
  EXPECT_EQ("http://example.com/", cmd_line.argv()[3]);

  // Expand some "${...}" variables.
  SetBrowserPath("${safari}");
  cmd_line = driver()->CreateCommandLine(GURL("http://example.com/"));
  EXPECT_EQ(4u, cmd_line.argv().size());
  EXPECT_EQ("open", cmd_line.argv()[0]);
  EXPECT_EQ("-a", cmd_line.argv()[1]);
  EXPECT_EQ("Safari", cmd_line.argv()[2]);
  EXPECT_EQ("http://example.com/", cmd_line.argv()[3]);

  // Path looks like an application name => use `open'.
  SetBrowserPath("Safari");
  cmd_line = driver()->CreateCommandLine(GURL("http://example.com/"));
  EXPECT_EQ(4u, cmd_line.argv().size());
  EXPECT_EQ("open", cmd_line.argv()[0]);
  EXPECT_EQ("-a", cmd_line.argv()[1]);
  EXPECT_EQ("Safari", cmd_line.argv()[2]);
  EXPECT_EQ("http://example.com/", cmd_line.argv()[3]);
}

TEST_F(AlternativeBrowserDriverTest, CreateCommandLineContainsUrl) {
  // Use `open(1)' to launch browser paths that aren't absolute.

  // Args come after `--args', e.g.:
  //     open -a Safari http://example.com/ --args abc def
  SetBrowserPath("");
  auto params = UTF8VectorToValueList({"abc", "def"});
  SetBrowserParameters(params);
  auto cmd_line = driver()->CreateCommandLine(GURL("http://example.com/"));
  EXPECT_EQ(7u, cmd_line.argv().size());
  EXPECT_EQ("open", cmd_line.argv()[0]);
  EXPECT_EQ("-a", cmd_line.argv()[1]);
  EXPECT_EQ("Safari", cmd_line.argv()[2]);
  EXPECT_EQ("http://example.com/", cmd_line.argv()[3]);
  EXPECT_EQ("--args", cmd_line.argv()[4]);
  EXPECT_EQ("abc", cmd_line.argv()[5]);
  EXPECT_EQ("def", cmd_line.argv()[6]);

  // If args contain "${url}", only put the URL together with the other
  // args. e.g.:
  //     open -a Safari --args abc http://example.com/ def
  params = UTF8VectorToValueList({"abc", "${url}", "def"});
  SetBrowserParameters(params);
  cmd_line = driver()->CreateCommandLine(GURL("http://example.com/"));
  EXPECT_EQ(7u, cmd_line.argv().size());
  EXPECT_EQ("open", cmd_line.argv()[0]);
  EXPECT_EQ("-a", cmd_line.argv()[1]);
  EXPECT_EQ("Safari", cmd_line.argv()[2]);
  EXPECT_EQ("--args", cmd_line.argv()[3]);
  EXPECT_EQ("abc", cmd_line.argv()[4]);
  EXPECT_EQ("http://example.com/", cmd_line.argv()[5]);
  EXPECT_EQ("def", cmd_line.argv()[6]);
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_POSIX)
TEST_F(AlternativeBrowserDriverTest, CreateCommandLineExpandsTilde) {
  setenv("HOME", "/home/foobar", true);

  SetBrowserPath("/usr/bin/true");
  auto params = UTF8VectorToValueList({"~/file.txt", "/tmp/backup.txt~"});
  SetBrowserParameters(params);
  auto cmd_line = driver()->CreateCommandLine(GURL("http://example.com/"));
  const base::CommandLine::StringVector& argv = cmd_line.argv();
  EXPECT_EQ(4u, argv.size());
  EXPECT_EQ(UTF8ToNative("/usr/bin/true"), argv[0]);
  EXPECT_EQ(UTF8ToNative("/home/foobar/file.txt"), argv[1]);
  EXPECT_EQ(UTF8ToNative("/tmp/backup.txt~"), argv[2]);
  EXPECT_EQ(UTF8ToNative("http://example.com/"), argv[3]);
}

TEST_F(AlternativeBrowserDriverTest, CreateCommandLineExpandsEnvVars) {
  setenv("A", "AAA", true);
  setenv("B", "BBB", true);
  setenv("CC", "CCC", true);
  setenv("D", "DDD", true);

  SetBrowserPath("/usr/bin/true");
  auto params = UTF8VectorToValueList(
      {"$A", "${B}", "before_${CC}_between_${D}_after", "$NONEXISTENT"});
  SetBrowserParameters(params);
  auto cmd_line = driver()->CreateCommandLine(GURL("http://example.com/"));
  const base::CommandLine::StringVector& argv = cmd_line.argv();
  EXPECT_EQ(6u, argv.size());
  EXPECT_EQ(UTF8ToNative("/usr/bin/true"), argv[0]);
  EXPECT_EQ(UTF8ToNative("AAA"), argv[1]);
  EXPECT_EQ(UTF8ToNative("BBB"), argv[2]);
  EXPECT_EQ("before_CCC_between_DDD_after", argv[3]);
  EXPECT_EQ("", argv[4]);
  EXPECT_EQ(UTF8ToNative("http://example.com/"), argv[5]);
}

TEST_F(AlternativeBrowserDriverTest, CreateCommandLineDoesntExpandUrlContent) {
  setenv("A", "AAA", true);
  setenv("B", "BBB", true);

  SetBrowserPath("/usr/bin/true");
  auto cmd_line = driver()->CreateCommandLine(GURL("http://evil.com/$A${B}"));
  EXPECT_EQ(2u, cmd_line.argv().size());
  EXPECT_EQ(UTF8ToNative("/usr/bin/true"), cmd_line.argv()[0]);
  EXPECT_EQ("http://evil.com/$A$%7BB%7D", cmd_line.argv()[1]);

  auto params = UTF8VectorToValueList({"${url}"});
  SetBrowserParameters(params);
  cmd_line = driver()->CreateCommandLine(GURL("http://evil.com/$A${B}"));
  EXPECT_EQ(2u, cmd_line.argv().size());
  EXPECT_EQ(UTF8ToNative("/usr/bin/true"), cmd_line.argv()[0]);
  EXPECT_EQ("http://evil.com/$A$%7BB%7D", cmd_line.argv()[1]);
}
#endif  // BUILDFLAG(IS_POSIX)

}  // namespace browser_switcher
