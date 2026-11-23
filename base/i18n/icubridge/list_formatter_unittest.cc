// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/list_formatter.h"

#include <stddef.h>

#include <array>
#include <string>

#include "base/i18n/icu_util.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {

namespace {

// Distinct locales, more than the list formatter cache capacity (currently 8),
// so that requesting all of them evicts the least recently used one.
constexpr auto kMoreLocalesThanCacheCapacity = std::to_array<LanguageTag>({
    GetKnownLanguageTag("en-US"),
    GetKnownLanguageTag("fr"),
    GetKnownLanguageTag("de"),
    GetKnownLanguageTag("es"),
    GetKnownLanguageTag("it"),
    GetKnownLanguageTag("pt-BR"),
    GetKnownLanguageTag("ja"),
    GetKnownLanguageTag("ko"),
    GetKnownLanguageTag("zh-CN"),
});

// Requests the list formatter of every locale in
// `kMoreLocalesThanCacheCapacity` except the first one, so that the first one
// is evicted from the cache if it was requested just before.
void RequestAllLocalesButFirst() {
  for (size_t i = 1; i < kMoreLocalesThanCacheCapacity.size(); ++i) {
    IcuBridge::GetInstance().list_formatter(kMoreLocalesThanCacheCapacity[i]);
  }
}

}  // namespace

class ListFormatterTest : public testing::Test {
 public:
  void SetUp() override { InitializeICU(); }
};

// Tests that the three formatters return an empty string for an empty list.
TEST_F(ListFormatterTest, EmptyList) {
  scoped_refptr<IcuBridge::ListFormatter> list_formatter =
      IcuBridge::GetInstance().list_formatter();
  EXPECT_EQ(list_formatter->FormatStandardList({}), u"");
  EXPECT_EQ(list_formatter->FormatNarrowList({}), u"");
  EXPECT_EQ(list_formatter->FormatUnitList({}), u"");
}

// Tests that the three formatters return the item itself for a single item.
TEST_F(ListFormatterTest, SingleItem) {
  scoped_refptr<IcuBridge::ListFormatter> list_formatter =
      IcuBridge::GetInstance().list_formatter();
  const std::u16string kSingle[] = {u"One"};
  EXPECT_EQ(list_formatter->FormatStandardList(kSingle), u"One");
  EXPECT_EQ(list_formatter->FormatNarrowList(kSingle), u"One");
  EXPECT_EQ(list_formatter->FormatUnitList(kSingle), u"One");
}

// Tests the three formatters with several items in English.
TEST_F(ListFormatterTest, English) {
  ScopedDefaultIcuLocale scoped_locale(GetKnownLanguageTag("en-US"));
  const std::u16string kItems[] = {u"A", u"B", u"C"};
  scoped_refptr<IcuBridge::ListFormatter> list_formatter =
      IcuBridge::GetInstance().list_formatter();
  EXPECT_EQ(list_formatter->FormatNarrowList(kItems), u"A, B, C");
  // ICU follows the Unicode CLDR specification (see
  // https://www.unicode.org/reports/tr35/tr35-general.html#ListPatterns),
  // which specifies the serial (Oxford) comma in the standard conjunction
  // pattern for English: "{0}, and {1}". We follow ICU's implementation
  // directly.
  EXPECT_EQ(list_formatter->FormatStandardList(kItems), u"A, B, and C");
  EXPECT_EQ(list_formatter->FormatUnitList(kItems), u"A, B, C");
}

// Tests the three formatters with several items in Chinese.
TEST_F(ListFormatterTest, Chinese) {
  ScopedDefaultIcuLocale scoped_locale(GetKnownLanguageTag("zh-CN"));
  const std::u16string kItems[] = {u"A", u"B", u"C"};
  scoped_refptr<IcuBridge::ListFormatter> list_formatter =
      IcuBridge::GetInstance().list_formatter();
  EXPECT_EQ(list_formatter->FormatStandardList(kItems), u"A、B和C");
  EXPECT_EQ(list_formatter->FormatNarrowList(kItems), u"A、B、C");
  // In Chinese, unit list formatting produces unseparated sequence "ABC",
  // showing a difference between unit and narrow formatting.
  EXPECT_EQ(list_formatter->FormatUnitList(kItems), u"ABC");
}

// Tests the three formatters with several items in Japanese.
TEST_F(ListFormatterTest, Japanese) {
  ScopedDefaultIcuLocale scoped_locale(GetKnownLanguageTag("ja"));
  const std::u16string kItems[] = {u"A", u"B", u"C"};
  scoped_refptr<IcuBridge::ListFormatter> list_formatter =
      IcuBridge::GetInstance().list_formatter();
  EXPECT_EQ(list_formatter->FormatStandardList(kItems), u"A、B、C");
  EXPECT_EQ(list_formatter->FormatNarrowList(kItems), u"A、B、C");
  // In Japanese, unit list formatting separates items with spaces ("A B C")
  // rather than enumeration commas ("A、B、C"), showing a difference
  // between unit and narrow formatting.
  EXPECT_EQ(list_formatter->FormatUnitList(kItems), u"A B C");
}

// Tests the three formatters with several items in Arabic.
TEST_F(ListFormatterTest, Arabic) {
  ScopedDefaultIcuLocale scoped_locale(GetKnownLanguageTag("ar"));
  const std::u16string kItems[] = {u"A", u"B", u"C"};
  scoped_refptr<IcuBridge::ListFormatter> list_formatter =
      IcuBridge::GetInstance().list_formatter();
  EXPECT_EQ(list_formatter->FormatStandardList(kItems), u"A وB وC");
  EXPECT_EQ(list_formatter->FormatNarrowList(kItems), u"A وB وC");
  EXPECT_EQ(list_formatter->FormatUnitList(kItems), u"A، وB، وC");
}

// Tests that a formatter requested for an explicit locale uses that locale
// rather than the default ICU locale.
TEST_F(ListFormatterTest, ExplicitLocale) {
  ScopedDefaultIcuLocale scoped_locale(GetKnownLanguageTag("en-US"));
  scoped_refptr<IcuBridge::ListFormatter> list_formatter =
      IcuBridge::GetInstance().list_formatter(GetKnownLanguageTag("zh-CN"));
  const std::u16string kItems[] = {u"A", u"B", u"C"};
  EXPECT_EQ(list_formatter->FormatStandardList(kItems), u"A、B和C");
}

class IcuBridgeListFormatterCacheTest : public testing::Test {
 public:
  void SetUp() override { InitializeICU(); }
};

// Tests that requesting the same locale twice returns the cached formatter.
TEST_F(IcuBridgeListFormatterCacheTest, ReusesCachedFormatter) {
  scoped_refptr<IcuBridge::ListFormatter> first =
      IcuBridge::GetInstance().list_formatter(GetKnownLanguageTag("en-US"));
  scoped_refptr<IcuBridge::ListFormatter> second =
      IcuBridge::GetInstance().list_formatter(GetKnownLanguageTag("en-US"));
  EXPECT_EQ(first, second);
}

// Tests that a formatter held by the caller still works after it was evicted
// from the cache.
TEST_F(IcuBridgeListFormatterCacheTest, HeldFormatterWorksAfterEviction) {
  scoped_refptr<IcuBridge::ListFormatter> held =
      IcuBridge::GetInstance().list_formatter(kMoreLocalesThanCacheCapacity[0]);
  RequestAllLocalesButFirst();
  const std::u16string kItems[] = {u"A", u"B", u"C"};
  EXPECT_EQ(held->FormatStandardList(kItems), u"A, B, and C");
}

}  // namespace base::i18n
