// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/execute_js_tool_host.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/actor_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

TEST(ExecuteJsToolHostTest, SanitizeResponse_EnforcesConsoleByteCap) {
  auto response = mojom::ExecuteJsResponse::New(
      /*result=*/"42", /*truncated=*/false, /*js_error_message=*/std::nullopt,
      /*console_logs=*/std::string(kExecuteJsMaxConsoleBytes + 128, 'a'));

  ExecuteJsToolHost::SanitizeResponse(*response);

  EXPECT_EQ(response->console_logs.size(), kExecuteJsMaxConsoleBytes);
  EXPECT_EQ(response->result, "42");
  EXPECT_FALSE(response->truncated);
}

TEST(ExecuteJsToolHostTest,
     SanitizeResponse_TruncatesMultiByteUtf8ConsoleLogsCleanly) {
  std::string emojis;
  for (size_t i = 0; i < kExecuteJsMaxConsoleBytes; ++i) {
    emojis.append("\xF0\x9F\x98\x80");  // U+1F600 (4 UTF-8 bytes)
  }
  // Prepend 1 ASCII byte so the 4 KB cut falls mid-codepoint.
  std::string payload = "x" + emojis;
  auto response = mojom::ExecuteJsResponse::New(
      /*result=*/"", /*truncated=*/false, /*js_error_message=*/std::nullopt,
      /*console_logs=*/payload);

  ExecuteJsToolHost::SanitizeResponse(*response);

  EXPECT_LE(response->console_logs.size(), kExecuteJsMaxConsoleBytes);
  EXPECT_GT(response->console_logs.size(), kExecuteJsMaxConsoleBytes - 4);
  EXPECT_TRUE(base::IsStringUTF8(response->console_logs));
}

TEST(ExecuteJsToolHostTest, SanitizeResponse_EnforcesConsoleLineCap) {
  std::string many_lines;
  for (size_t i = 0; i < kExecuteJsMaxConsoleLines + 10; ++i) {
    if (i > 0) {
      many_lines.push_back('\n');
    }
    many_lines.append(base::StringPrintf("[log] line %zu", i));
  }
  auto response = mojom::ExecuteJsResponse::New(
      /*result=*/"", /*truncated=*/false, /*js_error_message=*/std::nullopt,
      /*console_logs=*/many_lines);

  ExecuteJsToolHost::SanitizeResponse(*response);

  std::vector<std::string> lines =
      base::SplitString(response->console_logs, "\n", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  EXPECT_EQ(lines.size(), kExecuteJsMaxConsoleLines);
  EXPECT_EQ(lines.front(), "[log] line 0");
  EXPECT_EQ(lines.back(), base::StringPrintf("[log] line %zu",
                                             kExecuteJsMaxConsoleLines - 1));
}

TEST(ExecuteJsToolHostTest, SanitizeResponse_PreservesEmptyConsoleLogs) {
  auto response = mojom::ExecuteJsResponse::New(
      /*result=*/"1", /*truncated=*/false, /*js_error_message=*/std::nullopt,
      /*console_logs=*/"");

  ExecuteJsToolHost::SanitizeResponse(*response);

  EXPECT_TRUE(response->console_logs.empty());
}

TEST(ExecuteJsToolHostTest, SanitizeResponse_DropsInvalidUtf8) {
  const std::string invalid_utf8 = "bad\xFF\xFEutf8";
  const std::string surrogate_utf8 = "bad\xED\xA0\x80utf8";

  // Invalid UTF-8 under the byte cap in console_logs, result, and error.
  {
    auto response = mojom::ExecuteJsResponse::New(
        /*result=*/invalid_utf8, /*truncated=*/true,
        /*js_error_message=*/invalid_utf8,
        /*console_logs=*/invalid_utf8);

    ExecuteJsToolHost::SanitizeResponse(*response);

    EXPECT_TRUE(response->result.empty());
    EXPECT_FALSE(response->truncated);
    EXPECT_FALSE(response->js_error_message.has_value());
    EXPECT_TRUE(response->console_logs.empty());
  }

  // Encoded UTF-16 surrogates are also rejected.
  {
    auto response = mojom::ExecuteJsResponse::New(
        /*result=*/surrogate_utf8, /*truncated=*/false,
        /*js_error_message=*/surrogate_utf8,
        /*console_logs=*/surrogate_utf8);

    ExecuteJsToolHost::SanitizeResponse(*response);

    EXPECT_TRUE(response->result.empty());
    EXPECT_FALSE(response->js_error_message.has_value());
    EXPECT_TRUE(response->console_logs.empty());
  }

  // Invalid UTF-8 earlier in a console_logs payload exceeding the byte cap.
  {
    std::string oversized_invalid =
        invalid_utf8 + std::string(kExecuteJsMaxConsoleBytes, 'a');
    auto response = mojom::ExecuteJsResponse::New(
        /*result=*/"", /*truncated=*/false, /*js_error_message=*/std::nullopt,
        /*console_logs=*/oversized_invalid);

    ExecuteJsToolHost::SanitizeResponse(*response);

    EXPECT_TRUE(response->console_logs.empty());
  }
}

TEST(ExecuteJsToolHostTest, SanitizeResponse_PreservesUnicodeNoncharacters) {
  // U+FFFF encoded as valid UTF-8 is emitted by V8 and accepted by protobuf
  // UTF-8 verification.
  const std::string with_nonchar = "prefix\xEF\xBF\xBFsuffix";
  auto response = mojom::ExecuteJsResponse::New(
      /*result=*/with_nonchar, /*truncated=*/false,
      /*js_error_message=*/with_nonchar,
      /*console_logs=*/with_nonchar);

  ExecuteJsToolHost::SanitizeResponse(*response);

  EXPECT_EQ(response->result, with_nonchar);
  EXPECT_EQ(response->js_error_message, with_nonchar);
  EXPECT_EQ(response->console_logs, with_nonchar);
}

TEST(ExecuteJsToolHostTest,
     SanitizeResponse_TruncatesOversizedUnicodeNoncharactersCleanly) {
  // Oversized payloads composed of U+FFFF (3 UTF-8 bytes) must truncate on a
  // UTF-8 boundary without stripping the noncharacters.
  auto make_nonchars = [](size_t min_bytes) {
    std::string out;
    out.reserve(min_bytes + 6);
    while (out.size() <= min_bytes) {
      out.append("\xEF\xBF\xBF");
    }
    return out;
  };
  auto response = mojom::ExecuteJsResponse::New(
      /*result=*/make_nonchars(kExecuteJsMaxResultBytes), /*truncated=*/false,
      /*js_error_message=*/make_nonchars(kExecuteJsMaxErrorMessageBytes),
      /*console_logs=*/make_nonchars(kExecuteJsMaxConsoleBytes));

  ExecuteJsToolHost::SanitizeResponse(*response);

  EXPECT_LE(response->result.size(), kExecuteJsMaxResultBytes);
  EXPECT_GT(response->result.size(), kExecuteJsMaxResultBytes - 3);
  EXPECT_EQ(response->result.size() % 3, 0u);
  EXPECT_TRUE(base::IsStringUTF8AllowingNoncharacters(response->result));
  EXPECT_TRUE(response->truncated);

  ASSERT_TRUE(response->js_error_message.has_value());
  EXPECT_LE(response->js_error_message->size(), kExecuteJsMaxErrorMessageBytes);
  EXPECT_GT(response->js_error_message->size(),
            kExecuteJsMaxErrorMessageBytes - 3);
  EXPECT_EQ(response->js_error_message->size() % 3, 0u);
  EXPECT_TRUE(
      base::IsStringUTF8AllowingNoncharacters(*response->js_error_message));

  EXPECT_LE(response->console_logs.size(), kExecuteJsMaxConsoleBytes);
  EXPECT_GT(response->console_logs.size(), kExecuteJsMaxConsoleBytes - 3);
  EXPECT_EQ(response->console_logs.size() % 3, 0u);
  EXPECT_TRUE(base::IsStringUTF8AllowingNoncharacters(response->console_logs));
}

TEST(ExecuteJsToolHostTest, SanitizeResponse_EnforcesResultAndErrorByteCaps) {
  std::string oversized_result(kExecuteJsMaxResultBytes + 64, 'r');
  std::string oversized_error(kExecuteJsMaxErrorMessageBytes + 64, 'e');
  auto response = mojom::ExecuteJsResponse::New(
      /*result=*/oversized_result, /*truncated=*/false,
      /*js_error_message=*/oversized_error,
      /*console_logs=*/"");

  ExecuteJsToolHost::SanitizeResponse(*response);

  EXPECT_EQ(response->result.size(), kExecuteJsMaxResultBytes);
  EXPECT_TRUE(response->truncated);
  ASSERT_TRUE(response->js_error_message.has_value());
  EXPECT_EQ(response->js_error_message->size(), kExecuteJsMaxErrorMessageBytes);
}

TEST(ExecuteJsToolHostTest, SanitizeResponse_PreservesExactLimits) {
  std::string exact_lines;
  for (size_t i = 0; i < kExecuteJsMaxConsoleLines; ++i) {
    if (i > 0) {
      exact_lines.push_back('\n');
    }
    exact_lines.append("x");
  }
  auto response = mojom::ExecuteJsResponse::New(
      /*result=*/"\"ok\"", /*truncated=*/false,
      /*js_error_message=*/"Error: msg",
      /*console_logs=*/exact_lines);

  ExecuteJsToolHost::SanitizeResponse(*response);

  EXPECT_EQ(response->result, "\"ok\"");
  EXPECT_FALSE(response->truncated);
  EXPECT_EQ(response->js_error_message, "Error: msg");
  EXPECT_EQ(response->console_logs, exact_lines);
}

TEST(ExecuteJsToolHostTest,
     SanitizeResponse_EnforcesCombinedConsoleByteAndLineCaps) {
  // Exceeds both byte and line caps, hitting the byte cap first.
  {
    std::string long_many_lines;
    for (size_t i = 0; i < kExecuteJsMaxConsoleLines + 10; ++i) {
      if (i > 0) {
        long_many_lines.push_back('\n');
      }
      long_many_lines.append(200, 'a');
    }
    auto response = mojom::ExecuteJsResponse::New(
        /*result=*/"", /*truncated=*/false, /*js_error_message=*/std::nullopt,
        /*console_logs=*/long_many_lines);

    ExecuteJsToolHost::SanitizeResponse(*response);

    EXPECT_LE(response->console_logs.size(), kExecuteJsMaxConsoleBytes);
    std::vector<std::string> lines =
        base::SplitString(response->console_logs, "\n", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_ALL);
    EXPECT_LE(lines.size(), kExecuteJsMaxConsoleLines);
    EXPECT_TRUE(base::IsStringUTF8(response->console_logs));
  }

  // Exceeds both byte and line caps, hitting the line cap first.
  {
    std::string short_prefix_huge_tail;
    for (size_t i = 0; i < kExecuteJsMaxConsoleLines; ++i) {
      if (i > 0) {
        short_prefix_huge_tail.push_back('\n');
      }
      short_prefix_huge_tail.append(base::StringPrintf("[log] line %zu", i));
    }
    short_prefix_huge_tail.push_back('\n');
    short_prefix_huge_tail.append(kExecuteJsMaxConsoleBytes, 'z');
    auto response = mojom::ExecuteJsResponse::New(
        /*result=*/"", /*truncated=*/false, /*js_error_message=*/std::nullopt,
        /*console_logs=*/short_prefix_huge_tail);

    ExecuteJsToolHost::SanitizeResponse(*response);

    EXPECT_LE(response->console_logs.size(), kExecuteJsMaxConsoleBytes);
    std::vector<std::string> lines =
        base::SplitString(response->console_logs, "\n", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_ALL);
    EXPECT_EQ(lines.size(), kExecuteJsMaxConsoleLines);
    EXPECT_EQ(lines.front(), "[log] line 0");
    EXPECT_EQ(lines.back(), base::StringPrintf("[log] line %zu",
                                               kExecuteJsMaxConsoleLines - 1));
  }
}

TEST(ExecuteJsToolHostTest, SanitizeResponse_PreservesIncomingTruncatedFlag) {
  auto response = mojom::ExecuteJsResponse::New(
      /*result=*/"partial", /*truncated=*/true,
      /*js_error_message=*/std::nullopt,
      /*console_logs=*/"");

  ExecuteJsToolHost::SanitizeResponse(*response);

  EXPECT_EQ(response->result, "partial");
  EXPECT_TRUE(response->truncated);
}

}  // namespace
}  // namespace actor
