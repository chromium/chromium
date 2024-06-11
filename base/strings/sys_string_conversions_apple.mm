// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/sys_string_conversions.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include <string_view>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/numerics/safe_conversions.h"

namespace base {

namespace {

// Converts the supplied CFString into the specified encoding, and returns it as
// a C++ library string of the template type. Returns an empty string on
// failure.
//
// Do not assert in this function since it is used by the assertion code!
template <typename StringType>
StringType CFStringToStringWithEncodingT(CFStringRef cfstring,
                                         CFStringEncoding encoding) {
  CFIndex length = CFStringGetLength(cfstring);
  if (length == 0) {
    return StringType();
  }

  CFRange whole_string = CFRangeMake(0, length);
  CFIndex out_size;
  CFIndex converted = CFStringGetBytes(cfstring, whole_string, encoding,
                                       /*lossByte=*/0,
                                       /*isExternalRepresentation=*/false,
                                       /*buffer=*/nullptr,
                                       /*maxBufLen=*/0, &out_size);
  if (converted == 0 || out_size <= 0) {
    return StringType();
  }

  // `out_size` is the number of UInt8-sized units needed in the destination.
  // A buffer allocated as UInt8 units might not be properly aligned to
  // contain elements of StringType::value_type.  Use a container for the
  // proper value_type, and convert `out_size` by figuring the number of
  // value_type elements per UInt8.  Leave room for a NUL terminator.
  size_t elements = static_cast<size_t>(out_size) * sizeof(UInt8) /
                        sizeof(typename StringType::value_type) +
                    1;

  std::vector<typename StringType::value_type> out_buffer(elements);
  converted =
      CFStringGetBytes(cfstring, whole_string, encoding,
                       /*lossByte=*/0,
                       /*isExternalRepresentation=*/false,
                       reinterpret_cast<UInt8*>(&out_buffer[0]), out_size,
                       /*usedBufLen=*/nullptr);
  if (converted == 0) {
    return StringType();
  }

  out_buffer[elements - 1] = '\0';
  return StringType(&out_buffer[0], elements - 1);
}

// Given a C++ library string `in` with an encoding specified by `in_encoding`,
// converts it to `out_encoding` and returns it as a C++ library string of the
// `OutStringType` template type. Returns an empty string on failure.
//
// Do not assert in this function since it is used by the assertion code!
template <typename InStringType, typename OutStringType>
OutStringType StringToStringWithEncodingsT(const InStringType& in,
                                           CFStringEncoding in_encoding,
                                           CFStringEncoding out_encoding) {
  typename InStringType::size_type in_length = in.length();
  if (in_length == 0) {
    return OutStringType();
  }

  apple::ScopedCFTypeRef<CFStringRef> cfstring(CFStringCreateWithBytesNoCopy(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(in.data()),
      checked_cast<CFIndex>(in_length *
                            sizeof(typename InStringType::value_type)),
      in_encoding,
      /*isExternalRepresentation=*/false, kCFAllocatorNull));
  if (!cfstring) {
    return OutStringType();
  }

  return CFStringToStringWithEncodingT<OutStringType>(cfstring.get(),
                                                      out_encoding);
}

// Given a std::string_view `in` with an encoding specified by `in_encoding`,
// returns it as a CFStringRef. Returns null on failure.
template <typename CharT>
apple::ScopedCFTypeRef<CFStringRef> StringPieceToCFStringWithEncodingsT(
    std::basic_string_view<CharT> in,
    CFStringEncoding in_encoding) {
  const auto in_length = in.length();
  if (in_length == 0) {
    return apple::ScopedCFTypeRef<CFStringRef>(CFSTR(""),
                                               base::scoped_policy::RETAIN);
  }

  return apple::ScopedCFTypeRef<CFStringRef>(CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(in.data()),
      checked_cast<CFIndex>(in_length * sizeof(CharT)), in_encoding, false));
}

}  // namespace

// The CFStringEncodings used below specify the byte ordering explicitly,
// otherwise CFString will be confused when strings don't carry BOMs, as they
// typically won't.

// Do not assert in this function since it is used by the assertion code!
std::string SysWideToUTF8(const std::wstring& wide) {
  return StringToStringWithEncodingsT<std::wstring, std::string>(
      wide, kCFStringEncodingUTF32LE, kCFStringEncodingUTF8);
}

// Do not assert in this function since it is used by the assertion code!
std::wstring SysUTF8ToWide(std::string_view utf8) {
  return StringToStringWithEncodingsT<std::string_view, std::wstring>(
      utf8, kCFStringEncodingUTF8, kCFStringEncodingUTF32LE);
}

std::string SysWideToNativeMB(const std::wstring& wide) {
  return SysWideToUTF8(wide);
}

std::wstring SysNativeMBToWide(std::string_view native_mb) {
  return SysUTF8ToWide(native_mb);
}

apple::ScopedCFTypeRef<CFStringRef> SysUTF8ToCFStringRef(
    std::string_view utf8) {
  return StringPieceToCFStringWithEncodingsT(utf8, kCFStringEncodingUTF8);
}

apple::ScopedCFTypeRef<CFStringRef> SysUTF16ToCFStringRef(
    std::u16string_view utf16) {
  return StringPieceToCFStringWithEncodingsT(utf16, kCFStringEncodingUTF16LE);
}

NSString* SysUTF8ToNSString(std::string_view utf8) {
  return base::apple::CFToNSOwnershipCast(SysUTF8ToCFStringRef(utf8).release());
}

NSString* SysUTF16ToNSString(std::u16string_view utf16) {
  return base::apple::CFToNSOwnershipCast(
      SysUTF16ToCFStringRef(utf16).release());
}

std::string SysCFStringRefToUTF8(CFStringRef ref) {
  return CFStringToStringWithEncodingT<std::string>(ref, kCFStringEncodingUTF8);
}

std::u16string SysCFStringRefToUTF16(CFStringRef ref) {
  return CFStringToStringWithEncodingT<std::u16string>(
      ref, kCFStringEncodingUTF16LE);
}

std::string SysNSStringToUTF8(NSString* nsstring) {
  if (!nsstring) {
    return std::string();
  }
  return SysCFStringRefToUTF8(apple::NSToCFPtrCast(nsstring));
}

std::u16string SysNSStringToUTF16(NSString* nsstring) {
  if (!nsstring) {
    return std::u16string();
  }
  return SysCFStringRefToUTF16(apple::NSToCFPtrCast(nsstring));
}

}  // namespace base
