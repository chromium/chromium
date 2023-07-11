// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_FOUNDATION_UTIL_H_
#define BASE_MAC_FOUNDATION_UTIL_H_

#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <Security/Security.h>

#include <string>

#include "base/base_export.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "build/build_config.h"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
@class NSFont;
@class UIFont;
#endif  // __OBJC__

namespace base {
class FilePath;
}

namespace base::mac {

// Returns true if the application is running from a bundle
BASE_EXPORT bool AmIBundled();
BASE_EXPORT void SetOverrideAmIBundled(bool value);

#if defined(UNIT_TEST)
// This is required because instantiating some tests requires checking the
// directory structure, which sets the AmIBundled cache state. Individual tests
// may or may not be bundled, and this would trip them up if the cache weren't
// cleared. This should not be called from individual tests, just from test
// instantiation code that gets a path from PathService.
BASE_EXPORT void ClearAmIBundledCache();
#endif

// Returns true if this process is marked as a "Background only process".
BASE_EXPORT bool IsBackgroundOnlyProcess();

// Returns the path to a resource within the framework bundle.
BASE_EXPORT FilePath PathForFrameworkBundleResource(const char* resource_name);

// Returns the creator code associated with the CFBundleRef at bundle.
OSType CreatorCodeForCFBundleRef(CFBundleRef bundle);

// Returns the creator code associated with this application, by calling
// CreatorCodeForCFBundleRef for the application's main bundle.  If this
// information cannot be determined, returns kUnknownType ('????').  This
// does not respect the override app bundle because it's based on CFBundle
// instead of NSBundle, and because callers probably don't want the override
// app bundle's creator code anyway.
BASE_EXPORT OSType CreatorCodeForApplication();

#if defined(__OBJC__)

// Searches for directories for the given key in only the given |domain_mask|.
// If found, fills result (which must always be non-NULL) with the
// first found directory and returns true.  Otherwise, returns false.
BASE_EXPORT bool GetSearchPathDirectory(NSSearchPathDirectory directory,
                                        NSSearchPathDomainMask domain_mask,
                                        FilePath* result);

// Searches for directories for the given key in only the local domain.
// If found, fills result (which must always be non-NULL) with the
// first found directory and returns true.  Otherwise, returns false.
BASE_EXPORT bool GetLocalDirectory(NSSearchPathDirectory directory,
                                   FilePath* result);

// Searches for directories for the given key in only the user domain.
// If found, fills result (which must always be non-NULL) with the
// first found directory and returns true.  Otherwise, returns false.
BASE_EXPORT bool GetUserDirectory(NSSearchPathDirectory directory,
                                  FilePath* result);

#endif  // __OBJC__

// Returns the ~/Library directory.
BASE_EXPORT FilePath GetUserLibraryPath();

// Returns the ~/Documents directory.
BASE_EXPORT FilePath GetUserDocumentPath();

// Takes a path to an (executable) binary and tries to provide the path to an
// application bundle containing it. It takes the outermost bundle that it can
// find (so for "/Foo/Bar.app/.../Baz.app/..." it produces "/Foo/Bar.app").
//   |exec_name| - path to the binary
//   returns - path to the application bundle, or empty on error
BASE_EXPORT FilePath GetAppBundlePath(const FilePath& exec_name);

// Takes a path to an (executable) binary and tries to provide the path to an
// application bundle containing it. It takes the innermost bundle that it can
// find (so for "/Foo/Bar.app/.../Baz.app/..." it produces
// "/Foo/Bar.app/.../Baz.app").
//   |exec_name| - path to the binary
//   returns - path to the application bundle, or empty on error
BASE_EXPORT FilePath GetInnermostAppBundlePath(const FilePath& exec_name);

#define TYPE_NAME_FOR_CF_TYPE_DECL(TypeCF) \
  BASE_EXPORT std::string TypeNameForCFType(TypeCF##Ref)

TYPE_NAME_FOR_CF_TYPE_DECL(CFArray);
TYPE_NAME_FOR_CF_TYPE_DECL(CFBag);
TYPE_NAME_FOR_CF_TYPE_DECL(CFBoolean);
TYPE_NAME_FOR_CF_TYPE_DECL(CFData);
TYPE_NAME_FOR_CF_TYPE_DECL(CFDate);
TYPE_NAME_FOR_CF_TYPE_DECL(CFDictionary);
TYPE_NAME_FOR_CF_TYPE_DECL(CFNull);
TYPE_NAME_FOR_CF_TYPE_DECL(CFNumber);
TYPE_NAME_FOR_CF_TYPE_DECL(CFSet);
TYPE_NAME_FOR_CF_TYPE_DECL(CFString);
TYPE_NAME_FOR_CF_TYPE_DECL(CFURL);
TYPE_NAME_FOR_CF_TYPE_DECL(CFUUID);

TYPE_NAME_FOR_CF_TYPE_DECL(CGColor);

TYPE_NAME_FOR_CF_TYPE_DECL(CTFont);
TYPE_NAME_FOR_CF_TYPE_DECL(CTRun);

TYPE_NAME_FOR_CF_TYPE_DECL(SecAccessControl);
TYPE_NAME_FOR_CF_TYPE_DECL(SecCertificate);
TYPE_NAME_FOR_CF_TYPE_DECL(SecKey);
TYPE_NAME_FOR_CF_TYPE_DECL(SecPolicy);

#undef TYPE_NAME_FOR_CF_TYPE_DECL

// Returns the base bundle ID, which can be set by SetBaseBundleID but
// defaults to a reasonable string. This never returns NULL. BaseBundleID
// returns a pointer to static storage that must not be freed.
BASE_EXPORT const char* BaseBundleID();

// Sets the base bundle ID to override the default. The implementation will
// make its own copy of new_base_bundle_id.
BASE_EXPORT void SetBaseBundleID(const char* new_base_bundle_id);

// CFCast<>() and CFCastStrict<>() cast a basic CFTypeRef to a more
// specific CoreFoundation type. The compatibility of the passed
// object is found by comparing its opaque type against the
// requested type identifier. If the supplied object is not
// compatible with the requested return type, CFCast<>() returns
// NULL and CFCastStrict<>() will DCHECK. Providing a NULL pointer
// to either variant results in NULL being returned without
// triggering any DCHECK.
//
// Example usage:
// CFNumberRef some_number = base::mac::CFCast<CFNumberRef>(
//     CFArrayGetValueAtIndex(array, index));
//
// CFTypeRef hello = CFSTR("hello world");
// CFStringRef some_string = base::mac::CFCastStrict<CFStringRef>(hello);

template<typename T>
T CFCast(const CFTypeRef& cf_val);

template<typename T>
T CFCastStrict(const CFTypeRef& cf_val);

#define CF_CAST_DECL(TypeCF)                                            \
  template <>                                                           \
  BASE_EXPORT TypeCF##Ref CFCast<TypeCF##Ref>(const CFTypeRef& cf_val); \
                                                                        \
  template <>                                                           \
  BASE_EXPORT TypeCF##Ref CFCastStrict<TypeCF##Ref>(const CFTypeRef& cf_val)

CF_CAST_DECL(CFArray);
CF_CAST_DECL(CFBag);
CF_CAST_DECL(CFBoolean);
CF_CAST_DECL(CFData);
CF_CAST_DECL(CFDate);
CF_CAST_DECL(CFDictionary);
CF_CAST_DECL(CFNull);
CF_CAST_DECL(CFNumber);
CF_CAST_DECL(CFSet);
CF_CAST_DECL(CFString);
CF_CAST_DECL(CFURL);
CF_CAST_DECL(CFUUID);

CF_CAST_DECL(CGColor);

CF_CAST_DECL(CTFont);
CF_CAST_DECL(CTFontDescriptor);
CF_CAST_DECL(CTRun);

CF_CAST_DECL(SecAccessControl);
CF_CAST_DECL(SecCertificate);
CF_CAST_DECL(SecKey);
CF_CAST_DECL(SecPolicy);

#undef CF_CAST_DECL

#if defined(__OBJC__)

// ObjCCast<>() and ObjCCastStrict<>() cast a basic id to a more
// specific (NSObject-derived) type. The compatibility of the passed
// object is found by checking if it's a kind of the requested type
// identifier. If the supplied object is not compatible with the
// requested return type, ObjCCast<>() returns nil and
// ObjCCastStrict<>() will DCHECK. Providing a nil pointer to either
// variant results in nil being returned without triggering any DCHECK.
//
// The strict variant is useful when retrieving a value from a
// collection which only has values of a specific type, e.g. an
// NSArray of NSStrings. The non-strict variant is useful when
// retrieving values from data that you can't fully control. For
// example, a plist read from disk may be beyond your exclusive
// control, so you'd only want to check that the values you retrieve
// from it are of the expected types, but not crash if they're not.
//
// Example usage:
// NSString* version = base::mac::ObjCCast<NSString>(
//     [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"]);
//
// NSString* str = base::mac::ObjCCastStrict<NSString>(
//     [ns_arr_of_ns_strs objectAtIndex:0]);
template<typename T>
T* ObjCCast(id objc_val) {
  if ([objc_val isKindOfClass:[T class]]) {
    return reinterpret_cast<T*>(objc_val);
  }
  return nil;
}

template<typename T>
T* ObjCCastStrict(id objc_val) {
  T* rv = ObjCCast<T>(objc_val);
  DCHECK(objc_val == nil || rv);
  return rv;
}

#endif  // defined(__OBJC__)

// Helper function for GetValueFromDictionary to create the error message
// that appears when a type mismatch is encountered.
BASE_EXPORT std::string GetValueFromDictionaryErrorMessage(
    CFStringRef key, const std::string& expected_type, CFTypeRef value);

// Utility function to pull out a value from a dictionary, check its type, and
// return it. Returns NULL if the key is not present or of the wrong type.
template<typename T>
T GetValueFromDictionary(CFDictionaryRef dict, CFStringRef key) {
  CFTypeRef value = CFDictionaryGetValue(dict, key);
  T value_specific = CFCast<T>(value);

  if (value && !value_specific) {
    std::string expected_type = TypeNameForCFType(value_specific);
    DLOG(WARNING) << GetValueFromDictionaryErrorMessage(key,
                                                        expected_type,
                                                        value);
  }

  return value_specific;
}

#if defined(__OBJC__)

// Converts |path| to an autoreleased NSURL. Returns nil if |path| is empty.
BASE_EXPORT NSURL* FilePathToNSURL(const FilePath& path);

// Converts |path| to an autoreleased NSString. Returns nil if |path| is empty.
BASE_EXPORT NSString* FilePathToNSString(const FilePath& path);

// Converts |str| to a FilePath. Returns an empty path if |str| is nil.
BASE_EXPORT FilePath NSStringToFilePath(NSString* str);

// Converts |url| to a FilePath. Returns an empty path if |url| is nil or if
// |url| is not of scheme "file".
BASE_EXPORT FilePath NSURLToFilePath(NSURL* url);

#endif  // __OBJC__

// Converts a non-null |path| to a CFURLRef. |path| must not be empty.
//
// This function only uses manually-owned resources, so it does not depend on an
// NSAutoreleasePool being set up on the current thread.
BASE_EXPORT ScopedCFTypeRef<CFURLRef> FilePathToCFURL(const FilePath& path);

#if defined(__OBJC__)
// Converts |range| to an NSRange, returning the new range in |range_out|.
// Returns true if conversion was successful, false if the values of |range|
// could not be converted to NSUIntegers.
[[nodiscard]] BASE_EXPORT bool CFRangeToNSRange(CFRange range,
                                                NSRange* range_out);
#endif  // defined(__OBJC__)

}  // namespace base::mac

// Stream operations for CFTypes. They can be used with Objective-C types as
// well by using the casting methods in base/apple/bridging.h.
//
// For example: LOG(INFO) << base::apple::NSToCFPtrCast(@"foo");
//
// operator<<() can not be overloaded for Objective-C types as the compiler
// cannot distinguish between overloads for id with overloads for void*.
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o,
                                            const CFErrorRef err);
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o,
                                            const CFStringRef str);
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, CFRange);

#if defined(__OBJC__)
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, id);
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, NSRange);
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, SEL);

#if BUILDFLAG(IS_MAC)
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, NSPoint);
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, NSRect);
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, NSSize);
#endif  // IS_MAC

#endif  // __OBJC__

#endif  // BASE_MAC_FOUNDATION_UTIL_H_
