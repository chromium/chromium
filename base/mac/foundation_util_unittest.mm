// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/foundation_util.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <limits.h>
#include <stddef.h>

#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace base::mac {

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
      nullptr, nullptr, nullptr, 0, &kCFCopyStringDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
  ScopedCFTypeRef<CFTypeRef> test_dict_mutable(CFDictionaryCreateMutable(
      nullptr, 0, &kCFCopyStringDictionaryKeyCallBacks,
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
  EXPECT_EQ(test_array, CFCast<CFArrayRef>(test_array));
  EXPECT_EQ(test_array_mutable, CFCast<CFArrayRef>(test_array_mutable));
  EXPECT_EQ(test_bag, CFCast<CFBagRef>(test_bag));
  EXPECT_EQ(test_bag_mutable, CFCast<CFBagRef>(test_bag_mutable));
  EXPECT_EQ(test_bool, CFCast<CFBooleanRef>(test_bool));
  EXPECT_EQ(test_data, CFCast<CFDataRef>(test_data));
  EXPECT_EQ(test_data_mutable, CFCast<CFDataRef>(test_data_mutable));
  EXPECT_EQ(test_date, CFCast<CFDateRef>(test_date));
  EXPECT_EQ(test_dict, CFCast<CFDictionaryRef>(test_dict));
  EXPECT_EQ(test_dict_mutable, CFCast<CFDictionaryRef>(test_dict_mutable));
  EXPECT_EQ(test_number, CFCast<CFNumberRef>(test_number));
  EXPECT_EQ(test_null, CFCast<CFNullRef>(test_null));
  EXPECT_EQ(test_set, CFCast<CFSetRef>(test_set));
  EXPECT_EQ(test_set_mutable, CFCast<CFSetRef>(test_set_mutable));
  EXPECT_EQ(test_str, CFCast<CFStringRef>(test_str));
  EXPECT_EQ(test_str_const, CFCast<CFStringRef>(test_str_const));
  EXPECT_EQ(test_str_mutable, CFCast<CFStringRef>(test_str_mutable));

  // When given an incorrect CF cast, provide nullptr.
  EXPECT_FALSE(CFCast<CFStringRef>(test_array));
  EXPECT_FALSE(CFCast<CFStringRef>(test_array_mutable));
  EXPECT_FALSE(CFCast<CFStringRef>(test_bag));
  EXPECT_FALSE(CFCast<CFSetRef>(test_bag_mutable));
  EXPECT_FALSE(CFCast<CFSetRef>(test_bool));
  EXPECT_FALSE(CFCast<CFNullRef>(test_data));
  EXPECT_FALSE(CFCast<CFDictionaryRef>(test_data_mutable));
  EXPECT_FALSE(CFCast<CFDictionaryRef>(test_date));
  EXPECT_FALSE(CFCast<CFNumberRef>(test_dict));
  EXPECT_FALSE(CFCast<CFDateRef>(test_dict_mutable));
  EXPECT_FALSE(CFCast<CFDataRef>(test_number));
  EXPECT_FALSE(CFCast<CFDataRef>(test_null));
  EXPECT_FALSE(CFCast<CFBooleanRef>(test_set));
  EXPECT_FALSE(CFCast<CFBagRef>(test_set_mutable));
  EXPECT_FALSE(CFCast<CFBagRef>(test_str));
  EXPECT_FALSE(CFCast<CFArrayRef>(test_str_const));
  EXPECT_FALSE(CFCast<CFArrayRef>(test_str_mutable));

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
  EXPECT_EQ(test_array, CFCastStrict<CFArrayRef>(test_array));
  EXPECT_EQ(test_array_mutable, CFCastStrict<CFArrayRef>(test_array_mutable));
  EXPECT_EQ(test_bag, CFCastStrict<CFBagRef>(test_bag));
  EXPECT_EQ(test_bag_mutable, CFCastStrict<CFBagRef>(test_bag_mutable));
  EXPECT_EQ(test_bool, CFCastStrict<CFBooleanRef>(test_bool));
  EXPECT_EQ(test_data, CFCastStrict<CFDataRef>(test_data));
  EXPECT_EQ(test_data_mutable, CFCastStrict<CFDataRef>(test_data_mutable));
  EXPECT_EQ(test_date, CFCastStrict<CFDateRef>(test_date));
  EXPECT_EQ(test_dict, CFCastStrict<CFDictionaryRef>(test_dict));
  EXPECT_EQ(test_dict_mutable,
            CFCastStrict<CFDictionaryRef>(test_dict_mutable));
  EXPECT_EQ(test_number, CFCastStrict<CFNumberRef>(test_number));
  EXPECT_EQ(test_null, CFCastStrict<CFNullRef>(test_null));
  EXPECT_EQ(test_set, CFCastStrict<CFSetRef>(test_set));
  EXPECT_EQ(test_set_mutable, CFCastStrict<CFSetRef>(test_set_mutable));
  EXPECT_EQ(test_str, CFCastStrict<CFStringRef>(test_str));
  EXPECT_EQ(test_str_const, CFCastStrict<CFStringRef>(test_str_const));
  EXPECT_EQ(test_str_mutable, CFCastStrict<CFStringRef>(test_str_mutable));

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

  CFStringRef keys[] = { CFSTR("one"), CFSTR("two"), CFSTR("three") };
  CFNumberRef values[] = { cf_one, cf_two, cf_three };

  static_assert(std::size(keys) == std::size(values),
                "keys and values arrays must have the same size");

  ScopedCFTypeRef<CFDictionaryRef> test_dict(CFDictionaryCreate(
      kCFAllocatorDefault, reinterpret_cast<const void**>(keys),
      reinterpret_cast<const void**>(values), std::size(values),
      &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

  // GetValueFromDictionary<>(_, _) should produce the correct
  // expected output.
  EXPECT_EQ(values[0],
            GetValueFromDictionary<CFNumberRef>(test_dict, CFSTR("one")));
  EXPECT_EQ(values[1],
            GetValueFromDictionary<CFNumberRef>(test_dict, CFSTR("two")));
  EXPECT_EQ(values[2],
            GetValueFromDictionary<CFNumberRef>(test_dict, CFSTR("three")));

  // Bad input should produce bad output.
  EXPECT_FALSE(GetValueFromDictionary<CFNumberRef>(test_dict, CFSTR("four")));
  EXPECT_FALSE(GetValueFromDictionary<CFStringRef>(test_dict, CFSTR("one")));
}

TEST(FoundationUtilTest, FilePathToNSURL) {
  EXPECT_NSEQ(nil, FilePathToNSURL(FilePath()));
  EXPECT_NSEQ([NSURL fileURLWithPath:@"/a/b"],
              FilePathToNSURL(FilePath("/a/b")));
}

TEST(FoundationUtilTest, FilePathToNSString) {
  EXPECT_NSEQ(nil, FilePathToNSString(FilePath()));
  EXPECT_NSEQ(@"/a/b", FilePathToNSString(FilePath("/a/b")));
}

TEST(FoundationUtilTest, NSStringToFilePath) {
  EXPECT_EQ(FilePath(), NSStringToFilePath(nil));
  EXPECT_EQ(FilePath(), NSStringToFilePath(@""));
  EXPECT_EQ(FilePath("/a/b"), NSStringToFilePath(@"/a/b"));
}

TEST(FoundationUtilTest, FilePathToCFURL) {
  ScopedCFTypeRef<CFURLRef> url(CFURLCreateWithFileSystemPath(
      nullptr, CFSTR("/a/b"), kCFURLPOSIXPathStyle, false));
  EXPECT_TRUE(CFEqual(url.get(), FilePathToCFURL(FilePath("/a/b"))));
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

}  // namespace base::mac
