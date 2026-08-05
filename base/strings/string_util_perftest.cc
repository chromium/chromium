// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"

#include <cinttypes>

#include "base/features.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

template <typename String>
void MeasureIsStringASCII(size_t str_length, size_t non_ascii_pos) {
  String str(str_length, 'A');
  if (non_ascii_pos < str_length) {
    str[non_ascii_pos] = '\xAF';
  }

  TimeTicks t0 = TimeTicks::Now();
  for (size_t i = 0; i < 10000000; ++i) {
    IsStringASCII(str);
  }
  TimeDelta time = TimeTicks::Now() - t0;
  printf(
      "char-size:\t%zu\tlength:\t%zu\tnon-ascii-pos:\t%zu\ttime-ms:\t%" PRIu64
      "\n",
      sizeof(typename String::value_type), str_length, non_ascii_pos,
      time.InMilliseconds());
}

TEST(StringUtilTest, DISABLED_IsStringASCIIPerf) {
  for (size_t str_length = 4; str_length <= 1024; str_length *= 2) {
    for (size_t non_ascii_loc = 0; non_ascii_loc < 3; ++non_ascii_loc) {
      size_t non_ascii_pos = str_length * non_ascii_loc / 2 + 2;
      MeasureIsStringASCII<std::string>(str_length, non_ascii_pos);
      MeasureIsStringASCII<std::u16string>(str_length, non_ascii_pos);
#if defined(WCHAR_T_IS_32_BIT)
      MeasureIsStringASCII<std::basic_string<wchar_t>>(str_length,
                                                       non_ascii_pos);
#endif
    }
  }
}

void MeasureUTF8ToUTF16(const std::string& name,
                        std::string_view input,
                        bool enable_feature,
                        size_t iterations = 1000000) {
  test::ScopedFeatureList feature_list;
  if (enable_feature) {
    feature_list.InitAndEnableFeature(features::kUtfConversionAsciiFastPath);
  } else {
    feature_list.InitAndDisableFeature(features::kUtfConversionAsciiFastPath);
  }
  strings_internal::InitializeUtfStringConversionsFeatures();

  std::u16string output;
  TimeTicks t0 = TimeTicks::Now();
  for (size_t i = 0; i < iterations; ++i) {
    UTF8ToUTF16(input.data(), input.length(), &output);
  }
  TimeDelta time = TimeTicks::Now() - t0;
  uint64_t normalized_ms =
      iterations == 1000000
          ? static_cast<uint64_t>(time.InMilliseconds())
          : static_cast<uint64_t>(time.InMicroseconds() * 1000000 / iterations /
                                  1000);
  printf("UTF8ToUTF16 | mode: %-8s | dataset: %-42s | time-ms: %" PRIu64 "\n",
         enable_feature ? "FAST" : "LEGACY", name.c_str(), normalized_ms);
}

void MeasureUTF16ToUTF8(const std::string& name,
                        std::u16string_view input,
                        bool enable_feature,
                        size_t iterations = 1000000) {
  test::ScopedFeatureList feature_list;
  if (enable_feature) {
    feature_list.InitAndEnableFeature(features::kUtfConversionAsciiFastPath);
  } else {
    feature_list.InitAndDisableFeature(features::kUtfConversionAsciiFastPath);
  }
  strings_internal::InitializeUtfStringConversionsFeatures();

  std::string output;
  TimeTicks t0 = TimeTicks::Now();
  for (size_t i = 0; i < iterations; ++i) {
    UTF16ToUTF8(input.data(), input.length(), &output);
  }
  TimeDelta time = TimeTicks::Now() - t0;
  uint64_t normalized_ms =
      iterations == 1000000
          ? static_cast<uint64_t>(time.InMilliseconds())
          : static_cast<uint64_t>(time.InMicroseconds() * 1000000 / iterations /
                                  1000);
  printf("UTF16ToUTF8 | mode: %-8s | dataset: %-42s | time-ms: %" PRIu64 "\n",
         enable_feature ? "FAST" : "LEGACY", name.c_str(), normalized_ms);
}

void MeasureUTF8ToWide(const std::string& name,
                       std::string_view input,
                       bool enable_feature,
                       size_t iterations = 1000000) {
  test::ScopedFeatureList feature_list;
  if (enable_feature) {
    feature_list.InitAndEnableFeature(features::kUtfConversionAsciiFastPath);
  } else {
    feature_list.InitAndDisableFeature(features::kUtfConversionAsciiFastPath);
  }
  strings_internal::InitializeUtfStringConversionsFeatures();

  std::wstring output;
  TimeTicks t0 = TimeTicks::Now();
  for (size_t i = 0; i < iterations; ++i) {
    UTF8ToWide(input.data(), input.length(), &output);
  }
  TimeDelta time = TimeTicks::Now() - t0;
  uint64_t normalized_ms =
      iterations == 1000000
          ? static_cast<uint64_t>(time.InMilliseconds())
          : static_cast<uint64_t>(time.InMicroseconds() * 1000000 / iterations /
                                  1000);
  printf("UTF8ToWide  | mode: %-8s | dataset: %-42s | time-ms: %" PRIu64 "\n",
         enable_feature ? "FAST" : "LEGACY", name.c_str(), normalized_ms);
}

TEST(StringUtilTest, DISABLED_UTFConversionPerf) {
  std::string large_json_10k = "{\"results\": [";
  while (large_json_10k.length() < 10000) {
    large_json_10k +=
        "{\"id\": 1024, \"title\": \"Google 検索\", \"desc\": \"Bonjour le "
        "monde!\"},";
  }
  large_json_10k += "]}";

  std::string large_non_ascii_10k;
  while (large_non_ascii_10k.length() < 10000) {
    large_non_ascii_10k += "日本語の文章テキストデータです。";
  }

  struct Dataset {
    std::string name;
    std::string text;
    size_t iterations = 1000000;
  } datasets[] = {
      // 1. Very short strings (< 32 bytes)
      {"Short Pure ASCII (HTML ID)", "main-container", 1000000},
      {"Short Mixed ASCII (Token)", "id_café", 1000000},
      {"Short Pure Non-ASCII (CJK)", "検索", 1000000},

      // 2. Standard real-world mixed datasets
      {"Pure ASCII (URL)",
       "https://www.google.com/search?q=chromium+base+strings+performance",
       1000000},
      {"Mixed ASCII+Japanese (HTML)",
       "<div class=\"title\">Google 検索と AI 機能</div>", 1000000},
      {"Mixed ASCII+Spanish (JSON)",
       "{\"status\": \"success\", \"message\": \"Bonjour le monde! ¿Cómo "
       "estás?\"}",
       1000000},
      {"Mixed ASCII+Cyrillic (Path)",
       "/usr/share/doc/chromium/документация/README.md", 1000000},
      {"Mixed ASCII+Arabic (JSON)",
       "{\"lang\": \"ar\", \"label\": \"مرحبا بالعالم\", \"code\": 200}",
       1000000},
      {"Mixed ASCII+Hindi (Header)",
       "HTTP/1.1 200 OK\r\nX-Custom-Header: नमस्ते दुनिया\r\n\r\n", 1000000},
      {"Pure Non-ASCII (Japanese)", "日本語の文章テキストデータです。",
       1000000},

      // 3. Interspersed non-ASCII & Early non-ASCII in long strings
      {"Early Non-ASCII in 1KB JSON",
       "{\"name\": \"José\", \"payload\": \"" + std::string(1000, 'x') + "\"}",
       1000000},
      {"Interspersed Emoji/Accents",
       "Hello 👋 World 🌍 Chromium 🚀 UTF8 💡 Strings 🔍 Unicode 🌐 Speed 🏎️",
       1000000},

      // 4. Synthetic batch boundary tests
      {"Worst-Case (End of 1st Batch)",
       std::string(127, 'A') + "\xC2\xA2" + std::string(100, 'A'), 1000000},
      {"Worst-Case (End of 2nd Batch)",
       std::string(255, 'A') + "\xC2\xA2" + std::string(100, 'A'), 1000000},
      {"Worst-Case (End of 8th Batch)",
       std::string(1023, 'A') + "\xC2\xA2" + std::string(100, 'A'), 1000000},

      // 5. Large multi-kilobyte payloads (10KB - 50KB)
      {"Large Payload - 10KB JSON (Mixed)", large_json_10k, 100000},
      {"Large Payload - 10KB Non-ASCII (CJK)", large_non_ascii_10k, 100000},
      {"Large Payload - 50KB ASCII Document", std::string(50000, 'A'), 20000},
      {"Large Payload - 50KB (Worst-Case Suffix)",
       std::string(50000, 'A') + "\xC2\xA2" + std::string(100, 'A'), 20000},
  };

  for (const auto& ds : datasets) {
    MeasureUTF8ToUTF16(ds.name, ds.text, false, ds.iterations);
    MeasureUTF8ToUTF16(ds.name, ds.text, true, ds.iterations);
  }

  std::u16string u16_path = u"/usr/share/doc/chromium/документация/README.md";
  MeasureUTF16ToUTF8("Mixed ASCII+Cyrillic (Path)", u16_path, false);
  MeasureUTF16ToUTF8("Mixed ASCII+Cyrillic (Path)", u16_path, true);

  MeasureUTF8ToWide("Mixed ASCII+Cyrillic (Path)",
                    "/usr/share/doc/chromium/документация/README.md", false);
  MeasureUTF8ToWide("Mixed ASCII+Cyrillic (Path)",
                    "/usr/share/doc/chromium/документация/README.md", true);
}

}  // namespace base
