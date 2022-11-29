// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stdint.h>

#include <string>
#include <tuple>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace base {

namespace {

FilePath::StringType GenerateNativeString(FuzzedDataProvider& provider) {
  const std::string raw_string = provider.ConsumeRandomLengthString();
#if BUILDFLAG(IS_WIN)
  return UTF8ToWide(raw_string);
#else
  return raw_string;
#endif
}

bool IsValidExtension(const FilePath::StringType& text) {
  return text.empty() || text[0] == FilePath::kExtensionSeparator;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > 10 * 1000) {
    // Bail out on huge inputs to avoid spurious timeout or OOM reports.
    return 0;
  }
  FuzzedDataProvider provider(data, size);

  // Create a random path. Smoke-test its getters.
  const FilePath path(GenerateNativeString(provider));
  std::ignore = path.GetComponents();
  std::ignore = path.DirName();
  std::ignore = path.BaseName();
  std::ignore = path.Extension();
  std::ignore = path.FinalExtension();
  std::ignore = path.RemoveExtension();
  std::ignore = path.RemoveFinalExtension();
  std::ignore = path.IsAbsolute();
  std::ignore = path.IsNetwork();
  std::ignore = path.EndsWithSeparator();
  std::ignore = path.AsEndingWithSeparator();
  std::ignore = path.StripTrailingSeparators();
  std::ignore = path.ReferencesParent();
  std::ignore = path.LossyDisplayName();
  std::ignore = path.MaybeAsASCII();
  std::ignore = path.AsUTF8Unsafe();
  std::ignore = path.AsUTF16Unsafe();
  std::ignore = path.NormalizePathSeparators();

  // Smoke-test operations against a text.
  const auto text = GenerateNativeString(provider);
  std::ignore = path.InsertBeforeExtension(text);
  std::ignore = path.AddExtension(text);
  std::ignore = path.ReplaceExtension(text);
  if (IsValidExtension(text)) {
    std::ignore = path.MatchesExtension(text);
    std::ignore = path.MatchesFinalExtension(text);
  }
  // Check ASCII variants as well.
  const auto text_ascii = provider.ConsumeRandomLengthString();
  if (IsStringASCII(text_ascii)) {
    std::ignore = path.InsertBeforeExtensionASCII(text_ascii);
    std::ignore = path.AddExtensionASCII(text_ascii);
  }

  // Test Pickle roundtrip.
  Pickle pickle;
  path.WriteToPickle(&pickle);
  PickleIterator pickle_iterator(pickle);
  FilePath decoded;
  CHECK(decoded.ReadFromPickle(&pickle_iterator));
  CHECK_EQ(decoded, path);

  // Smoke-test operations against a second path.
  FilePath second_path(GenerateNativeString(provider));
  std::ignore = path.IsParent(second_path);
  if (!second_path.IsAbsolute())
    std::ignore = path.Append(second_path);
  FilePath relative_path;
  std::ignore = path.AppendRelativePath(second_path, &relative_path);

  return 0;
}

}  // namespace base
