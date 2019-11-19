// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/alternative_browser_driver.h"

#include <stdlib.h>

#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/grit/generated_resources.h"
#include "url/gurl.h"

#include "third_party/re2/src/re2/re2.h"

namespace browser_switcher {

namespace {

const char kUrlVarName[] = "${url}";

#if defined(OS_MACOSX)
const char kChromeExecutableName[] = "Google Chrome";
const char kFirefoxExecutableName[] = "Firefox";
const char kOperaExecutableName[] = "Opera";
const char kSafariExecutableName[] = "Safari";
#else
const char kChromeExecutableName[] = "google-chrome";
const char kFirefoxExecutableName[] = "firefox";
const char kOperaExecutableName[] = "opera";
#endif

const char kChromeVarName[] = "${chrome}";
const char kFirefoxVarName[] = "${firefox}";
const char kOperaVarName[] = "${opera}";
#if defined(OS_MACOSX)
const char kSafariVarName[] = "${safari}";
#endif

const struct {
  const char* var_name;
  const char* executable_name;
  const char* browser_name;
} kBrowserVarMappings[] = {
    {kChromeVarName, kChromeExecutableName, ""},
    {kFirefoxVarName, kFirefoxExecutableName, "Mozilla Firefox"},
    {kOperaVarName, kOperaExecutableName, "Opera"},
#if defined(OS_MACOSX)
    {kSafariVarName, kSafariExecutableName, "Safari"},
#endif
};

bool ExpandUrlVarName(std::string* arg, const GURL& url) {
  size_t url_index = arg->find(kUrlVarName);
  if (url_index == std::string::npos)
    return false;
  arg->replace(url_index, strlen(kUrlVarName), url.spec());
  return true;
}

void ExpandTilde(std::string* arg) {
  if (base::StartsWith(*arg, "~", base::CompareCase::SENSITIVE))
    arg->replace(0, 1, getenv("HOME"));
}

void ExpandEnvironmentVariables(std::string* arg) {
  static re2::LazyRE2 re = {
      "\\$\\{([a-zA-Z_][a-zA-Z_0-9]*)\\}|\\$([a-zA-Z_][a-zA-Z_0-9]*)"};
  std::string out;
  re2::StringPiece submatch[3] = {0};
  size_t start = 0;
  bool matched = false;
  while (re->Match(*arg, start, arg->size(), re2::RE2::Anchor::UNANCHORED,
                   submatch, base::size(submatch))) {
    out.append(*arg, start, submatch[0].data() - (arg->data() + start));
    if (submatch[0] == kUrlVarName) {
      // Don't treat '${url}' as an environment variable, leave it as is.
      out.append(kUrlVarName);
    } else {
      std::string var_name =
          (submatch[1].empty() ? submatch[2] : submatch[1]).as_string();
      const char* var_value = getenv(var_name.c_str());
      if (var_value != NULL)
        out.append(var_value);
    }
    start = submatch[0].end() - arg->data();
    matched = true;
  }
  if (!matched)
    return;
  out.append(arg->data() + start, arg->size() - start);
  std::swap(out, *arg);
}

#if defined(OS_MACOSX)
bool ContainsUrlVarName(const std::vector<std::string>& tokens) {
  return std::any_of(tokens.begin(), tokens.end(),
                     [](const std::string& token) {
                       return token.find(kUrlVarName) != std::string::npos;
                     });
}
#endif  // defined(OS_MACOSX)

void AppendCommandLineArguments(base::CommandLine* cmd_line,
                                const std::vector<std::string>& raw_args,
                                const GURL& url,
                                bool always_append_url) {
  bool contains_url = false;
  for (const auto& arg : raw_args) {
    std::string expanded_arg = arg;
    ExpandTilde(&expanded_arg);
    ExpandEnvironmentVariables(&expanded_arg);
    if (ExpandUrlVarName(&expanded_arg, url))
      contains_url = true;
    cmd_line->AppendArg(expanded_arg);
  }
  if (always_append_url && !contains_url)
    cmd_line->AppendArg(url.spec());
}

void ExpandPresetBrowsers(std::string* str) {
#if defined(OS_MACOSX)
  // Unlike most POSIX platforms, MacOS always has another browser than Chrome,
  // so admins don't have to explicitly configure one.
  if (str->empty()) {
    *str = kSafariExecutableName;
    return;
  }
#endif
  for (const auto& mapping : kBrowserVarMappings) {
    if (!str->compare(mapping.var_name)) {
      *str = mapping.executable_name;
      return;
    }
  }
}

}  // namespace

AlternativeBrowserDriver::~AlternativeBrowserDriver() {}

AlternativeBrowserDriverImpl::AlternativeBrowserDriverImpl(
    const BrowserSwitcherPrefs* prefs)
    : prefs_(prefs) {}

AlternativeBrowserDriverImpl::~AlternativeBrowserDriverImpl() {}

bool AlternativeBrowserDriverImpl::TryLaunch(const GURL& url) {
#if !defined(OS_MACOSX)
  if (prefs_->GetAlternativeBrowserPath().empty()) {
    LOG(ERROR) << "Alternative browser not configured. "
               << "Aborting browser switch.";
    return false;
  }
#endif

  VLOG(2) << "Launching alternative browser...";
  VLOG(2) << "  path = " << prefs_->GetAlternativeBrowserPath();
  VLOG(2) << "  url = " << url.spec();

  CHECK(url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile());

  auto cmd_line = CreateCommandLine(url);
  base::LaunchOptions options;
  // Don't close the alternative browser when Chrome exits.
  options.new_process_group = true;
  if (!base::LaunchProcess(cmd_line, options).IsValid()) {
    LOG(ERROR) << "Could not start the alternative browser!";
    return false;
  }
  return true;
}

std::string AlternativeBrowserDriverImpl::GetBrowserName() const {
  std::string path = prefs_->GetAlternativeBrowserPath();
#if defined(OS_MACOSX)
  // Unlike most POSIX platforms, MacOS always has another browser than Chrome,
  // so admins don't have to explicitly configure one.
  if (path.empty())
    path = kSafariVarName;
#endif
  for (const auto& mapping : kBrowserVarMappings) {
    if (!path.compare(mapping.var_name))
      return std::string(mapping.browser_name);
  }
  return std::string();
}

base::CommandLine AlternativeBrowserDriverImpl::CreateCommandLine(
    const GURL& url) {
  std::string path = prefs_->GetAlternativeBrowserPath();
  ExpandPresetBrowsers(&path);
  ExpandTilde(&path);
  ExpandEnvironmentVariables(&path);

  const std::vector<std::string>& params =
      prefs_->GetAlternativeBrowserParameters();

#if defined(OS_MACOSX)
  // On MacOS, if the path doesn't start with a '/', it's probably not an
  // executable path. It is probably a name for an application, e.g. "Safari" or
  // "Google Chrome". Those can be launched using the `open(1)' command.
  //
  // It may use the following syntax (first syntax):
  //     open -a <browser_path> <url> [--args <browser_params...>]
  //
  // Or, if |browser_params| contains "${url}" (second syntax):
  //     open -a <browser_path> --args <browser_params...>
  //
  // Safari only supports the first syntax.
  if (!path.empty() && path[0] != '/') {
    base::CommandLine cmd_line(std::vector<std::string>{"open"});
    cmd_line.AppendArg("-a");
    cmd_line.AppendArg(path);
    if (!ContainsUrlVarName(params)) {
      // First syntax.
      cmd_line.AppendArg(url.spec());
    }
    if (!params.empty()) {
      // First or second syntax, depending on what is in |browser_params_|.
      cmd_line.AppendArg("--args");
      AppendCommandLineArguments(&cmd_line, params, url,
                                 /* always_append_url */ false);
    }
    return cmd_line;
  }
#endif
  base::CommandLine cmd_line(std::vector<std::string>{path});
  AppendCommandLineArguments(&cmd_line, params, url,
                             /* always_append_url */ true);
  return cmd_line;
}

}  // namespace browser_switcher
