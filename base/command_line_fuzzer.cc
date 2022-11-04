// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <tuple>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace base {

namespace {

CommandLine::StringType GenerateNativeString(FuzzedDataProvider& provider) {
  const std::string raw_string = provider.ConsumeRandomLengthString();
#if BUILDFLAG(IS_WIN)
  return UTF8ToWide(raw_string);
#else
  return raw_string;
#endif
}

CommandLine::StringVector GenerateNativeStringVector(
    FuzzedDataProvider& provider) {
  CommandLine::StringVector strings(
      provider.ConsumeIntegralInRange<int>(0, 100));
  for (auto& item : strings)
    item = GenerateNativeString(provider);
  return strings;
}

FilePath GenerateFilePath(FuzzedDataProvider& provider) {
  return FilePath(GenerateNativeString(provider));
}

bool IsForbiddenSwitchCharacter(char c) {
  return IsAsciiWhitespace(c) || c == '=' || c != ToLowerASCII(c);
}

bool IsValidSwitchName(const std::string& text) {
  // This duplicates the logic in command_line.cc, but it's not exposed in form
  // of public interface.
  return !text.empty() && !ranges::any_of(text, IsForbiddenSwitchCharacter) &&
         !StartsWith(text, "-") && !StartsWith(text, "/");
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  // Create a randomly initialized command line.
  CommandLine command_line(CommandLine::NO_PROGRAM);
  switch (provider.ConsumeIntegralInRange<int>(0, 3)) {
    case 0:
      // Leave empty.
      break;
    case 1:
      command_line = CommandLine(GenerateFilePath(provider));
      break;
    case 2:
      command_line = CommandLine(GenerateNativeStringVector(provider));
      break;
    case 3:
#if BUILDFLAG(IS_WIN)
      command_line.ParseFromString(GenerateNativeString(provider));
#endif
      break;
  }

  // Do a few mutations of the command line.
  while (provider.remaining_bytes() > 0) {
    switch (provider.ConsumeIntegralInRange<int>(0, 4)) {
      case 0: {
        // Add a switch.
        std::string name = provider.ConsumeRandomLengthString();
        if (IsValidSwitchName(name)) {
          CommandLine::StringType value = GenerateNativeString(provider);
          command_line.AppendSwitchNative(name, value);
          CHECK(command_line.HasSwitch(name));
          CHECK(command_line.GetSwitchValueNative(name) == value);
        }
        break;
      }
      case 1: {
        // Remove a switch.
        std::string name = provider.ConsumeRandomLengthString();
        if (IsValidSwitchName(name)) {
          command_line.RemoveSwitch(name);
          CHECK(!command_line.HasSwitch(name));
          CHECK(command_line.GetSwitchValueNative(name).empty());
        }
        break;
      }
      case 2: {
        // Add an argument.
        CommandLine::StringType arg = GenerateNativeString(provider);
        if (!arg.empty() && IsStringASCII(arg))
          command_line.AppendArgNative(arg);
        break;
      }
      case 3: {
        // Add a wrapper.
        CommandLine::StringType wrapper = GenerateNativeString(provider);
        if (!wrapper.empty())
          command_line.PrependWrapper(wrapper);
        break;
      }
      case 4: {
        // Check a switch.
        std::string name = provider.ConsumeRandomLengthString();
        if (IsValidSwitchName(name)) {
          std::ignore = command_line.HasSwitch(name);
          std::ignore = command_line.GetSwitchValueNative(name);
        }
        break;
      }
    }

    // Smoke-test various accessors.
    std::ignore = command_line.GetCommandLineString();
    std::ignore = command_line.GetArgumentsString();
#if BUILDFLAG(IS_WIN)
    std::ignore = command_line.GetCommandLineStringForShell();
    std::ignore = command_line.GetCommandLineStringWithUnsafeInsertSequences();
#endif
  }

  return 0;
}

}  // namespace base
