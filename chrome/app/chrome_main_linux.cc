// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main_linux.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
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
