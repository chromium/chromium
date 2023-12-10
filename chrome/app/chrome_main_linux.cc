// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main_linux.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "chrome/common/channel_info.h"

namespace {

void AppendExtraArgsFromEnvVar(const std::string& env_var_name,
                               std::vector<std::string>& out_args) {
  std::string extra_args_str;
  auto environment = base::Environment::Create();
  if (environment->GetVar(env_var_name, &extra_args_str)) {
    base::StringTokenizer tokenizer(extra_args_str, base::kWhitespaceASCII);
    tokenizer.set_quote_chars("\"'");
    while (tokenizer.GetNext()) {
      std::string arg;
      const std::string& token = tokenizer.token();
      base::TrimString(token, "\"'", &arg);
      out_args.push_back(arg);
    }
  }
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Reads the contents of the "CHROME_VERSION_EXTRA" file at the specified path.
std::string ReadChromeVersionExtra(const base::FilePath& path) {
  std::string contents;
  // Don't read the file if it's unexpectedly large.
  if (!base::ReadFileToStringWithMaxSize(path, &contents, 128)) {
    return std::string();
  }

  return std::string(base::TrimWhitespaceASCII(contents, base::TRIM_ALL));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

void AppendExtraArgumentsToCommandLine(base::CommandLine* command_line) {
  constexpr char kExtraFlagsVarName[] = "CHROME_EXTRA_FLAGS";
  std::vector<std::string> extra_args = {""};

  AppendExtraArgsFromEnvVar(kExtraFlagsVarName, extra_args);
  std::string channel_suffix =
      chrome::GetChannelSuffixForExtraFlagsEnvVarName();
  if (!channel_suffix.empty()) {
    AppendExtraArgsFromEnvVar(kExtraFlagsVarName + channel_suffix, extra_args);
  }

  command_line->AppendArguments(base::CommandLine(extra_args),
                                /*include_program=*/false);
}

void PossiblyDetermineFallbackChromeChannel(const char* launched_binary_path) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  // If "CHROME_VERSION_EXTRA" is not set, then we may have been launched via
  // the "chrome" binary directly, instead of the "google-chrome" wrapper
  // script. Check for an adjacent "CHROME_VERSION_EXTRA" file for the channel.
  // Note: Child processes inherit environment variables, so once this is set
  // in the parent, child processes won't enter the if below.
  if (!env->HasVar("CHROME_VERSION_EXTRA")) {
    base::FilePath path = base::FilePath(launched_binary_path)
                              .DirName()
                              .Append("CHROME_VERSION_EXTRA");
    std::string channel = ReadChromeVersionExtra(path);
    if (!channel.empty()) {
      LOG(WARNING) << "Read channel " << channel << " from " << path;
      env->SetVar("CHROME_VERSION_EXTRA", channel);
    }
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
