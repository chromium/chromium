// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/foundation_util.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <limits.h>
#include <stddef.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using testing::ElementsAreArray;

namespace base::apple {

TEST(FoundationUtilTest, CFCast) {
  // Build out the CF types to be tested as empty containers.
  ScopedCFTypeRef<CFTypeRef> test_array(
      CFArrayCreate(nullptr, nullptr, 0, &kCFTypeArrayCallBacks));
  ScopedCFTypeRef<CFTypeRef> test_array_mutable(
      CFArrayCreateMutable(nullptr, 0, &kCFTypeArrayCallBacks));
  ScopedCFTypeRef<CFTypeRef> test_bag(
      CFBagCreate(nullptr, nullptr, 0, &kCFTypeBagCallBacks));
  ScopedCFTypeRef<CFTypeRef> test_bag_mutable(
      CFBagCreateMutable(nullptr, 0, &kCFTypeBagCallBacks));
  CFTypeRef test_bool = kCFBooleanTrue;
  ScopedCFTypeRef<CFTypeRef> test_data(CFDataCreate(nullptr, nullptr, 0));
  ScopedCFTypeRef<CFTypeRef> test_data_mutable(CFDataCreateMutable(nullptr, 0));
  ScopedCFTypeRef<CFTypeRef> test_date(CFDateCreate(nullptr, 0));
  ScopedCFTypeRef<CFTypeRef> test_dict(CFDictionaryCreate(
      nullptr, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
  ScopedCFTypeRef<CFTypeRef> test_dict_mutable(
      CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  int int_val = 256;
  ScopedCFTypeRef<CFTypeRef> test_number(
      CFNumberCreate(nullptr, kCFNumberIntType, &int_val));
  CFTypeRef test_null = kCFNull;
  ScopedCFTypeRef<CFTypeRef> test_set(
      CFSetCreate(nullptr, nullptr, 0, &kCFTypeSetCallBacks));
  ScopedCFTypeRef<CFTypeRef> test_set_mutable(
      CFSetCreateMutable(nullptr, 0, &kCFTypeSetCallBacks));
  ScopedCFTypeRef<CFTypeRef> test_str(CFStringCreateWithBytes(
      nullptr, nullptr, 0, kCFStringEncodingASCII, false));
  CFTypeRef test_str_const = CFSTR("hello");
  ScopedCFTypeRef<CFTypeRef> test_str_mutable(
      CFStringCreateMutable(nullptr, 0));

  // Make sure the allocations of CF types are good.
  EXPECT_TRUE(test_array);
  EXPECT_TRUE(test_array_mutable);
  EXPECT_TRUE(test_bag);
  EXPECT_TRUE(test_bag_mutable);
  EXPECT_TRUE(test_bool);
  EXPECT_TRUE(test_data);
  EXPECT_TRUE(test_data_mutable);
  EXPECT_TRUE(test_date);
  EXPECT_TRUE(test_dict);
  EXPECT_TRUE(test_dict_mutable);
  EXPECT_TRUE(test_number);
  EXPECT_TRUE(test_null);
  EXPECT_TRUE(test_set);
  EXPECT_TRUE(test_set_mutable);
  EXPECT_TRUE(test_str);
  EXPECT_TRUE(test_str_const);
  EXPECT_TRUE(test_str_mutable);

  // Casting the CFTypeRef objects correctly provides the same pointer.
  EXPECT_EQ(test_array.get(), CFCast<CFArrayRef>(test_array.get()));
  EXPECT_EQ(test_array_mutable.get(),
            CFCast<CFArrayRef>(test_array_mutable.get()));
  EXPECT_EQ(test_bag.get(), CFCast<CFBagRef>(test_bag.get()));
  EXPECT_EQ(test_bag_mutable.get(), CFCast<CFBagRef>(test_bag_mutable.get()));
  EXPECT_EQ(test_bool, CFCast<CFBooleanRef>(test_bool));
  EXPECT_EQ(test_data.get(), CFCast<CFDataRef>(test_data.get()));
  EXPECT_EQ(test_data_mutable.get(),
            CFCast<CFDataRef>(test_data_mutable.get()));
  EXPECT_EQ(test_date.get(), CFCast<CFDateRef>(test_date.get()));
  EXPECT_EQ(test_dict.get(), CFCast<CFDictionaryRef>(test_dict.get()));
  EXPECT_EQ(test_dict_mutable.get(),
            CFCast<CFDictionaryRef>(test_dict_mutable.get()));
  EXPECT_EQ(test_number.get(), CFCast<CFNumberRef>(test_number.get()));
  EXPECT_EQ(test_null, CFCast<CFNullRef>(test_null));
  EXPECT_EQ(test_set.get(), CFCast<CFSetRef>(test_set.get()));
  EXPECT_EQ(test_set_mutable.get(), CFCast<CFSetRef>(test_set_mutable.get()));
  EXPECT_EQ(test_str.get(), CFCast<CFStringRef>(test_str.get()));
  EXPECT_EQ(test_str_const, CFCast<CFStringRef>(test_str_const));
  EXPECT_EQ(test_str_mutable.get(),
            CFCast<CFStringRef>(test_str_mutable.get()));

  // When given an incorrect CF cast, provide nullptr.
  EXPECT_FALSE(CFCast<CFStringRef>(test_array.get()));
  EXPECT_FALSE(CFCast<CFStringRef>(test_array_mutable.get()));
  EXPECT_FALSE(CFCast<CFStringRef>(test_bag.get()));
  EXPECT_FALSE(CFCast<CFSetRef>(test_bag_mutable.get()));
  EXPECT_FALSE(CFCast<CFSetRef>(test_bool));
  EXPECT_FALSE(CFCast<CFNullRef>(test_data.get()));
  EXPECT_FALSE(CFCast<CFDictionaryRef>(test_data_mutable.get()));
  EXPECT_FALSE(CFCast<CFDictionaryRef>(test_date.get()));
  EXPECT_FALSE(CFCast<CFNumberRef>(test_dict.get()));
  EXPECT_FALSE(CFCast<CFDateRef>(test_dict_mutable.get()));
  EXPECT_FALSE(CFCast<CFDataRef>(test_number.get()));
  EXPECT_FALSE(CFCast<CFDataRef>(test_null));
  EXPECT_FALSE(CFCast<CFBooleanRef>(test_set.get()));
  EXPECT_FALSE(CFCast<CFBagRef>(test_set_mutable.get()));
  EXPECT_FALSE(CFCast<CFBagRef>(test_str.get()));
  EXPECT_FALSE(CFCast<CFArrayRef>(test_str_const));
  EXPECT_FALSE(CFCast<CFArrayRef>(test_str_mutable.get()));

  // Giving a nullptr provides a nullptr.
  EXPECT_FALSE(CFCast<CFArrayRef>(nullptr));
  EXPECT_FALSE(CFCast<CFBagRef>(nullptr));
  EXPECT_FALSE(CFCast<CFBooleanRef>(nullptr));
  EXPECT_FALSE(CFCast<CFDataRef>(nullptr));
  EXPECT_FALSE(CFCast<CFDateRef>(nullptr));
  EXPECT_FALSE(CFCast<CFDictionaryRef>(nullptr));
  EXPECT_FALSE(CFCast<CFNullRef>(nullptr));
  EXPECT_FALSE(CFCast<CFNumberRef>(nullptr));
  EXPECT_FALSE(CFCast<CFSetRef>(nullptr));
  EXPECT_FALSE(CFCast<CFStringRef>(nullptr));

  // CFCastStrict: correct cast results in correct pointer being returned.
  EXPECT_EQ(test_array.get(), CFCastStrict<CFArrayRef>(test_array.get()));
  EXPECT_EQ(test_array_mutable.get(),
            CFCastStrict<CFArrayRef>(test_array_mutable.get()));
  EXPECT_EQ(test_bag.get(), CFCastStrict<CFBagRef>(test_bag.get()));
  EXPECT_EQ(test_bag_mutable.get(),
            CFCastStrict<CFBagRef>(test_bag_mutable.get()));
  EXPECT_EQ(test_bool, CFCastStrict<CFBooleanRef>(test_bool));
  EXPECT_EQ(test_data.get(), CFCastStrict<CFDataRef>(test_data.get()));
  EXPECT_EQ(test_data_mutable.get(),
            CFCastStrict<CFDataRef>(test_data_mutable.get()));
  EXPECT_EQ(test_date.get(), CFCastStrict<CFDateRef>(test_date.get()));
  EXPECT_EQ(test_dict.get(), CFCastStrict<CFDictionaryRef>(test_dict.get()));
  EXPECT_EQ(test_dict_mutable.get(),
            CFCastStrict<CFDictionaryRef>(test_dict_mutable.get()));
  EXPECT_EQ(test_number.get(), CFCastStrict<CFNumberRef>(test_number.get()));
  EXPECT_EQ(test_null, CFCastStrict<CFNullRef>(test_null));
  EXPECT_EQ(test_set.get(), CFCastStrict<CFSetRef>(test_set.get()));
  EXPECT_EQ(test_set_mutable.get(),
            CFCastStrict<CFSetRef>(test_set_mutable.get()));
  EXPECT_EQ(test_str.get(), CFCastStrict<CFStringRef>(test_str.get()));
  EXPECT_EQ(test_str_const, CFCastStrict<CFStringRef>(test_str_const));
  EXPECT_EQ(test_str_mutable.get(),
            CFCastStrict<CFStringRef>(test_str_mutable.get()));

  // CFCastStrict: Giving a nullptr provides a nullptr.
  EXPECT_FALSE(CFCastStrict<CFArrayRef>(nullptr));
  EXPECT_FALSE(CFCastStrict<CFBagRef>(nullptr));
  EXPECT_FALSE(CFCastStrict<CFBooleanRef>(nullptr));
  EXPECT_FALSE(CFCastStrict<CFDataRef>(nullptr));
  EXPECT_FALSE(CFCastStrict<CFDateRef>(nullptr));
  EXPECT_FALSE(CFCastStrict<CFDictionaryRef>(nullptr));
  EXPECT_FALSE(CFCastStrict<CFNullRef>(nullptr));
  EXPECT_FALSE(CFCastStrict<CFNumberRef>(nullptr));
  EXPECT_FALSE(CFCastStrict<CFSetRef>(nullptr));
  EXPECT_FALSE(CFCastStrict<CFStringRef>(nullptr));
}

TEST(FoundationUtilTest, ObjCCast) {
  @autoreleasepool {
    id test_array = @[];
    id test_array_mutable = [NSMutableArray array];
    id test_data = [NSData data];
    id test_data_mutable = [NSMutableData dataWithCapacity:10];
    id test_date = [NSDate date];
    id test_dict = @{@"meaning" : @42};
    id test_dict_mutable = [NSMutableDictionary dictionaryWithCapacity:10];
    id test_number = @42;
    id test_null = [NSNull null];
    id test_set = [NSSet setWithObject:@"string object"];
    id test_set_mutable = [NSMutableSet setWithCapacity:10];
    id test_str = [NSString string];
    id test_str_const = @"bonjour";
    id test_str_mutable = [NSMutableString stringWithCapacity:10];

    // Make sure the allocations of NS types are good.
    EXPECT_TRUE(test_array);
    EXPECT_TRUE(test_array_mutable);
    EXPECT_TRUE(test_data);
    EXPECT_TRUE(test_data_mutable);
    EXPECT_TRUE(test_date);
    EXPECT_TRUE(test_dict);
    EXPECT_TRUE(test_dict_mutable);
    EXPECT_TRUE(test_number);
    EXPECT_TRUE(test_null);
    EXPECT_TRUE(test_set);
    EXPECT_TRUE(test_set_mutable);
    EXPECT_TRUE(test_str);
    EXPECT_TRUE(test_str_const);
    EXPECT_TRUE(test_str_mutable);

    // Casting the id correctly provides the same pointer.
    EXPECT_EQ(test_array, ObjCCast<NSArray>(test_array));
    EXPECT_EQ(test_array_mutable, ObjCCast<NSArray>(test_array_mutable));
    EXPECT_EQ(test_data, ObjCCast<NSData>(test_data));
    EXPECT_EQ(test_data_mutable, ObjCCast<NSData>(test_data_mutable));
    EXPECT_EQ(test_date, ObjCCast<NSDate>(test_date));
    EXPECT_EQ(test_dict, ObjCCast<NSDictionary>(test_dict));
    EXPECT_EQ(test_dict_mutable, ObjCCast<NSDictionary>(test_dict_mutable));
    EXPECT_EQ(test_number, ObjCCast<NSNumber>(test_number));
    EXPECT_EQ(test_null, ObjCCast<NSNull>(test_null));
    EXPECT_EQ(test_set, ObjCCast<NSSet>(test_set));
    EXPECT_EQ(test_set_mutable, ObjCCast<NSSet>(test_set_mutable));
    EXPECT_EQ(test_str, ObjCCast<NSString>(test_str));
    EXPECT_EQ(test_str_const, ObjCCast<NSString>(test_str_const));
    EXPECT_EQ(test_str_mutable, ObjCCast<NSString>(test_str_mutable));

    // When given an incorrect ObjC cast, provide nil.
    EXPECT_FALSE(ObjCCast<NSString>(test_array));
    EXPECT_FALSE(ObjCCast<NSString>(test_array_mutable));
    EXPECT_FALSE(ObjCCast<NSString>(test_data));
    EXPECT_FALSE(ObjCCast<NSString>(test_data_mutable));
    EXPECT_FALSE(ObjCCast<NSSet>(test_date));
    EXPECT_FALSE(ObjCCast<NSSet>(test_dict));
    EXPECT_FALSE(ObjCCast<NSNumber>(test_dict_mutable));
    EXPECT_FALSE(ObjCCast<NSNull>(test_number));
    EXPECT_FALSE(ObjCCast<NSDictionary>(test_null));
    EXPECT_FALSE(ObjCCast<NSDictionary>(test_set));
    EXPECT_FALSE(ObjCCast<NSDate>(test_set_mutable));
    EXPECT_FALSE(ObjCCast<NSData>(test_str));
    EXPECT_FALSE(ObjCCast<NSData>(test_str_const));
    EXPECT_FALSE(ObjCCast<NSArray>(test_str_mutable));

    // Giving a nil provides a nil.
    EXPECT_FALSE(ObjCCast<NSArray>(nil));
    EXPECT_FALSE(ObjCCast<NSData>(nil));
    EXPECT_FALSE(ObjCCast<NSDate>(nil));
    EXPECT_FALSE(ObjCCast<NSDictionary>(nil));
    EXPECT_FALSE(ObjCCast<NSNull>(nil));
    EXPECT_FALSE(ObjCCast<NSNumber>(nil));
    EXPECT_FALSE(ObjCCast<NSSet>(nil));
    EXPECT_FALSE(ObjCCast<NSString>(nil));

    // ObjCCastStrict: correct cast results in correct pointer being returned.
    EXPECT_EQ(test_array, ObjCCastStrict<NSArray>(test_array));
    EXPECT_EQ(test_array_mutable, ObjCCastStrict<NSArray>(test_array_mutable));
    EXPECT_EQ(test_data, ObjCCastStrict<NSData>(test_data));
    EXPECT_EQ(test_data_mutable, ObjCCastStrict<NSData>(test_data_mutable));
    EXPECT_EQ(test_date, ObjCCastStrict<NSDate>(test_date));
    EXPECT_EQ(test_dict, ObjCCastStrict<NSDictionary>(test_dict));
    EXPECT_EQ(test_dict_mutable,
              ObjCCastStrict<NSDictionary>(test_dict_mutable));
    EXPECT_EQ(test_number, ObjCCastStrict<NSNumber>(test_number));
    EXPECT_EQ(test_null, ObjCCastStrict<NSNull>(test_null));
    EXPECT_EQ(test_set, ObjCCastStrict<NSSet>(test_set));
    EXPECT_EQ(test_set_mutable, ObjCCastStrict<NSSet>(test_set_mutable));
    EXPECT_EQ(test_str, ObjCCastStrict<NSString>(test_str));
    EXPECT_EQ(test_str_const, ObjCCastStrict<NSString>(test_str_const));
    EXPECT_EQ(test_str_mutable, ObjCCastStrict<NSString>(test_str_mutable));

    // ObjCCastStrict: Giving a nil provides a nil.
    EXPECT_FALSE(ObjCCastStrict<NSArray>(nil));
    EXPECT_FALSE(ObjCCastStrict<NSData>(nil));
    EXPECT_FALSE(ObjCCastStrict<NSDate>(nil));
    EXPECT_FALSE(ObjCCastStrict<NSDictionary>(nil));
    EXPECT_FALSE(ObjCCastStrict<NSNull>(nil));
    EXPECT_FALSE(ObjCCastStrict<NSNumber>(nil));
    EXPECT_FALSE(ObjCCastStrict<NSSet>(nil));
    EXPECT_FALSE(ObjCCastStrict<NSString>(nil));
  }
}

TEST(FoundationUtilTest, GetValueFromDictionary) {
  int one = 1, two = 2, three = 3;

  ScopedCFTypeRef<CFNumberRef> cf_one(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &one));
  ScopedCFTypeRef<CFNumberRef> cf_two(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &two));
  ScopedCFTypeRef<CFNumberRef> cf_three(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &three));

  CFStringRef keys[] = {CFSTR("one"), CFSTR("two"), CFSTR("three")};
  CFNumberRef values[] = {cf_one.get(), cf_two.get(), cf_three.get()};

  static_assert(std::size(keys) == std::size(values),
                "keys and values arrays must have the same size");

  ScopedCFTypeRef<CFDictionaryRef> test_dict(CFDictionaryCreate(
      kCFAllocatorDefault, reinterpret_cast<const void**>(keys),
      reinterpret_cast<const void**>(values), std::size(values),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

  // GetValueFromDictionary<>(_, _) should produce the correct
  // expected output.
  EXPECT_EQ(values[0],
            GetValueFromDictionary<CFNumberRef>(test_dict.get(), CFSTR("one")));
  EXPECT_EQ(values[1],
            GetValueFromDictionary<CFNumberRef>(test_dict.get(), CFSTR("two")));
  EXPECT_EQ(values[2], GetValueFromDictionary<CFNumberRef>(test_dict.get(),
                                                           CFSTR("three")));

  // Bad input should produce bad output.
  EXPECT_FALSE(
      GetValueFromDictionary<CFNumberRef>(test_dict.get(), CFSTR("four")));
  EXPECT_FALSE(
      GetValueFromDictionary<CFStringRef>(test_dict.get(), CFSTR("one")));
}

TEST(FoundationUtilTest, FilePathToNSURL) {
  EXPECT_NSEQ(nil, FilePathToNSURL(FilePath()));
  EXPECT_NSEQ([NSURL fileURLWithPath:@"/a/b"],
              FilePathToNSURL(FilePath("/a/b")));
  EXPECT_NSEQ([NSURL fileURLWithPath:@"a/b"], FilePathToNSURL(FilePath("a/b")));
}

TEST(FoundationUtilTest, FilePathToNSString) {
  EXPECT_NSEQ(nil, FilePathToNSString(FilePath()));
  EXPECT_NSEQ(@"/a/b", FilePathToNSString(FilePath("/a/b")));
  EXPECT_NSEQ(@"a/b", FilePathToNSString(FilePath("a/b")));
}

TEST(FoundationUtilTest, NSStringToFilePath) {
  EXPECT_EQ(FilePath(), NSStringToFilePath(nil));
  EXPECT_EQ(FilePath(), NSStringToFilePath(@""));
  EXPECT_EQ(FilePath("/a/b"), NSStringToFilePath(@"/a/b"));
  EXPECT_EQ(FilePath("a/b"), NSStringToFilePath(@"a/b"));
}

TEST(FoundationUtilTest, NSURLToFilePath) {
  EXPECT_EQ(FilePath(), NSURLToFilePath(nil));
  EXPECT_EQ(FilePath(),
            NSURLToFilePath([NSURL URLWithString:@"http://google.com/"]));
  EXPECT_EQ(FilePath("a/b"), NSURLToFilePath([NSURL fileURLWithPath:@"a/b"]));
  EXPECT_EQ(FilePath("/a/b"), NSURLToFilePath([NSURL fileURLWithPath:@"/a/b"]));
}

TEST(FoundationUtilTest, ConversionComposition) {
  // macOS "file system representation" is a bespoke Unicode decomposition.
  // Verifying that U+00E9 (LATIN SMALL LETTER E WITH ACUTE) ends up decomposed
  // into U+0065 (LATIN SMALL LETTER E) and U+0301 (COMBINING ACUTE ACCENT) is a
  // decent one-off test to ensure that this is happening.
  FilePath original_path("\u00E9");

  FilePath result_string_path =
      NSStringToFilePath(FilePathToNSString(original_path));
  EXPECT_EQ("\u0065\u0301", result_string_path.value());

  FilePath result_url_path = NSURLToFilePath(FilePathToNSURL(original_path));
  EXPECT_EQ("\u0065\u0301", result_url_path.value());
}

TEST(FoundationUtilTest, FilePathToCFURL) {
  EXPECT_EQ(ScopedCFTypeRef<CFURLRef>(), FilePathToCFURL(FilePath()));
  ScopedCFTypeRef<CFURLRef> absolute_url(CFURLCreateWithFileSystemPath(
      nullptr, CFSTR("/a/b"), kCFURLPOSIXPathStyle, /*isDirectory=*/false));
  EXPECT_TRUE(
      CFEqual(absolute_url.get(), FilePathToCFURL(FilePath("/a/b")).get()));
  ScopedCFTypeRef<CFURLRef> relative_url(CFURLCreateWithFileSystemPath(
      nullptr, CFSTR("a/b"), kCFURLPOSIXPathStyle, /*isDirectory=*/false));
  EXPECT_TRUE(
      CFEqual(relative_url.get(), FilePathToCFURL(FilePath("a/b")).get()));
}

TEST(FoundationUtilTest, FilePathToCFString) {
  EXPECT_EQ(ScopedCFTypeRef<CFStringRef>(), FilePathToCFString(FilePath()));
  EXPECT_TRUE(
      CFEqual(CFSTR("/a/b"), FilePathToCFString(FilePath("/a/b")).get()));
  EXPECT_TRUE(CFEqual(CFSTR("a/b"), FilePathToCFString(FilePath("a/b")).get()));
}

TEST(FoundationUtilTest, CFStringToFilePath) {
  EXPECT_EQ(FilePath(), CFStringToFilePath(nil));
  EXPECT_EQ(FilePath(), CFStringToFilePath(CFSTR("")));
  EXPECT_EQ(FilePath("/a/b"), CFStringToFilePath(CFSTR("/a/b")));
  EXPECT_EQ(FilePath("a/b"), CFStringToFilePath(CFSTR("a/b")));
}

TEST(FoundationUtilTest, CFURLToFilePath) {
  EXPECT_EQ(FilePath(), CFURLToFilePath(nil));
  ScopedCFTypeRef<CFURLRef> non_file_url(
      CFURLCreateWithString(nullptr, CFSTR("http://google.com/"), nullptr));
  EXPECT_EQ(FilePath(), CFURLToFilePath(non_file_url.get()));
  ScopedCFTypeRef<CFURLRef> absolute_url(CFURLCreateWithFileSystemPath(
      nullptr, CFSTR("/a/b"), kCFURLPOSIXPathStyle, /*isDirectory=*/false));
  EXPECT_EQ(FilePath("/a/b"), CFURLToFilePath(absolute_url.get()));
  ScopedCFTypeRef<CFURLRef> relative_url(CFURLCreateWithFileSystemPath(
      nullptr, CFSTR("a/b"), kCFURLPOSIXPathStyle, /*isDirectory=*/false));
  EXPECT_EQ(FilePath("a/b"), CFURLToFilePath(relative_url.get()));
}

TEST(FoundationUtilTest, CFRangeToNSRange) {
  NSRange range_out;
  EXPECT_TRUE(CFRangeToNSRange(CFRangeMake(10, 5), &range_out));
  EXPECT_EQ(10UL, range_out.location);
  EXPECT_EQ(5UL, range_out.length);
  EXPECT_FALSE(CFRangeToNSRange(CFRangeMake(-1, 5), &range_out));
  EXPECT_FALSE(CFRangeToNSRange(CFRangeMake(5, -1), &range_out));
  EXPECT_FALSE(CFRangeToNSRange(CFRangeMake(-1, -1), &range_out));
  EXPECT_FALSE(CFRangeToNSRange(CFRangeMake(LONG_MAX, LONG_MAX), &range_out));
  EXPECT_FALSE(CFRangeToNSRange(CFRangeMake(LONG_MIN, LONG_MAX), &range_out));
}

TEST(StringNumberConversionsTest, FormatNSInteger) {
  // The PRI[dxu]NS macro assumes that NSInteger is a typedef to "int" on
  // 32-bit architecture and a typedef to "long" on 64-bit architecture
  // (respectively "unsigned int" and "unsigned long" for NSUInteger). Use
  // pointer incompatibility to validate this at compilation.
#if defined(ARCH_CPU_64_BITS)
  typedef long FormatNSIntegerAsType;
  typedef unsigned long FormatNSUIntegerAsType;
#else
  typedef int FormatNSIntegerAsType;
  typedef unsigned int FormatNSUIntegerAsType;
#endif  // defined(ARCH_CPU_64_BITS)

  NSInteger some_nsinteger;
  [[maybe_unused]] FormatNSIntegerAsType* pointer_to_some_nsinteger =
      &some_nsinteger;

  NSUInteger some_nsuinteger;
  [[maybe_unused]] FormatNSUIntegerAsType* pointer_to_some_nsuinteger =
      &some_nsuinteger;

  // Check that format specifier works correctly for NSInteger.
  const struct {
    NSInteger value;
    const char* expected;
    const char* expected_hex;
  } nsinteger_cases[] = {
#if !defined(ARCH_CPU_64_BITS)
    {12345678, "12345678", "bc614e"},
    {-12345678, "-12345678", "ff439eb2"},
#else
    {12345678, "12345678", "bc614e"},
    {-12345678, "-12345678", "ffffffffff439eb2"},
    {137451299150l, "137451299150", "2000bc614e"},
    {-137451299150l, "-137451299150", "ffffffdfff439eb2"},
#endif  // !defined(ARCH_CPU_64_BITS)
  };

  for (const auto& nsinteger_case : nsinteger_cases) {
    EXPECT_EQ(nsinteger_case.expected,
              StringPrintf("%" PRIdNS, nsinteger_case.value));
    EXPECT_EQ(nsinteger_case.expected_hex,
              StringPrintf("%" PRIxNS, nsinteger_case.value));
  }

  // Check that format specifier works correctly for NSUInteger.
  const struct {
    NSUInteger value;
    const char* expected;
    const char* expected_hex;
  } nsuinteger_cases[] = {
#if !defined(ARCH_CPU_64_BITS)
    {12345678u, "12345678", "bc614e"},
    {4282621618u, "4282621618", "ff439eb2"},
#else
    {12345678u, "12345678", "bc614e"},
    {4282621618u, "4282621618", "ff439eb2"},
    {137451299150ul, "137451299150", "2000bc614e"},
    {18446743936258252466ul, "18446743936258252466", "ffffffdfff439eb2"},
#endif  // !defined(ARCH_CPU_64_BITS)
  };

  for (const auto& nsuinteger_case : nsuinteger_cases) {
    EXPECT_EQ(nsuinteger_case.expected,
              StringPrintf("%" PRIuNS, nsuinteger_case.value));
    EXPECT_EQ(nsuinteger_case.expected_hex,
              StringPrintf("%" PRIxNS, nsuinteger_case.value));
  }
}

TEST(FoundationUtilTest, NSDataToSpan) {
  {
    NSData* data = [NSData data];
    span<const uint8_t> span = NSDataToSpan(data);
    EXPECT_TRUE(span.empty());
  }

  {
    NSMutableData* data = [NSMutableData data];
    span<uint8_t> span = NSMutableDataToSpan(data);
    EXPECT_TRUE(span.empty());
  }

  const char buffer[4] = {0, CHAR_MAX, 0, CHAR_MAX};

  {
    NSData* data = [NSData dataWithBytes:buffer length:sizeof(buffer)];
    span<const uint8_t> span = NSDataToSpan(data);
    EXPECT_THAT(span, ElementsAreArray(buffer));
  }

  {
    NSMutableData* data = [NSMutableData dataWithBytes:buffer
                                                length:sizeof(buffer)];
    span<uint8_t> span = NSMutableDataToSpan(data);
    EXPECT_THAT(span, ElementsAreArray(buffer));
    span[0] = 123;
    EXPECT_EQ(static_cast<const char*>(data.bytes)[0], 123);
  }
}

TEST(FoundationUtilTest, CFDataToSpan) {
  {
    ScopedCFTypeRef<CFDataRef> data(CFDataCreate(nullptr, nullptr, 0));
    span<const uint8_t> span = CFDataToSpan(data.get());
    EXPECT_TRUE(span.empty());
  }

  {
    ScopedCFTypeRef<CFMutableDataRef> data(CFDataCreateMutable(nullptr, 0));
    span<uint8_t> span = CFMutableDataToSpan(data.get());
    EXPECT_TRUE(span.empty());
  }

  const uint8_t buffer[4] = {0, CHAR_MAX, 0, CHAR_MAX};

  {
    ScopedCFTypeRef<CFDataRef> data(
        CFDataCreate(nullptr, buffer, sizeof(buffer)));
    span<const uint8_t> span = CFDataToSpan(data.get());
    EXPECT_EQ(make_span(buffer), span);
    EXPECT_THAT(span, ElementsAreArray(buffer));
  }

  {
    ScopedCFTypeRef<CFMutableDataRef> data(CFDataCreateMutable(nullptr, 0));
    CFDataAppendBytes(data.get(), buffer, sizeof(buffer));
    span<uint8_t> span = CFMutableDataToSpan(data.get());
    EXPECT_EQ(make_span(buffer), span);
    span[0] = 123;
    EXPECT_EQ(CFDataGetBytePtr(data.get())[0], 123);
  }
}

#define EXPECT_LOG_EQ(expected, val) \
  EXPECT_EQ(expected, (std::ostringstream() << (val)).str())

TEST(FoundationLoggingTest, ObjCObject) {
  EXPECT_LOG_EQ("Hello, world!", @"Hello, world!");
}

TEST(FoundationLoggingTest, ObjCNil) {
  EXPECT_LOG_EQ("(nil)", static_cast<id>(nil));
}

TEST(FoundationLoggingTest, CFRange) {
  EXPECT_LOG_EQ("{0, 100}", CFRangeMake(0, 100));
}

TEST(FoundationLoggingTest, NSRange) {
  EXPECT_LOG_EQ("{0, 100}", NSMakeRange(0, 100));
}

}  // namespace base::apple
