// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/foundation_util.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_logging.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

#if !defined(OS_IOS)
#import <AppKit/AppKit.h>
#endif

extern "C" {
CFTypeID SecKeyGetTypeID();
#if !defined(OS_IOS)
CFTypeID SecACLGetTypeID();
CFTypeID SecTrustedApplicationGetTypeID();
// The NSFont/CTFont toll-free bridging is broken before 10.15.
// http://www.openradar.me/15341349 rdar://15341349
//
// TODO(https://crbug.com/1076527): This is fixed in 10.15. When 10.15 is the
// minimum OS for Chromium, remove this SPI declaration.
Boolean _CFIsObjC(CFTypeID typeID, CFTypeRef obj);
#endif
}  // extern "C"

namespace base {
namespace mac {

namespace {

bool g_cached_am_i_bundled_called = false;
bool g_cached_am_i_bundled_value = false;
bool g_override_am_i_bundled = false;
bool g_override_am_i_bundled_value = false;

bool UncachedAmIBundled() {
#if defined(OS_IOS)
  // All apps are bundled on iOS.
  return true;
#else
  if (g_override_am_i_bundled)
    return g_override_am_i_bundled_value;

  // Yes, this is cheap.
  return [[base::mac::OuterBundle() bundlePath] hasSuffix:@".app"];
#endif
}

}  // namespace

bool AmIBundled() {
  // If the return value is not cached, this function will return different
  // values depending on when it's called. This confuses some client code, see
  // http://crbug.com/63183 .
  if (!g_cached_am_i_bundled_called) {
    g_cached_am_i_bundled_called = true;
    g_cached_am_i_bundled_value = UncachedAmIBundled();
  }
  DCHECK_EQ(g_cached_am_i_bundled_value, UncachedAmIBundled())
      << "The return value of AmIBundled() changed. This will confuse tests. "
      << "Call SetAmIBundled() override manually if your test binary "
      << "delay-loads the framework.";
  return g_cached_am_i_bundled_value;
}

void SetOverrideAmIBundled(bool value) {
#if defined(OS_IOS)
  // It doesn't make sense not to be bundled on iOS.
  if (!value)
    NOTREACHED();
#endif
  g_override_am_i_bundled = true;
  g_override_am_i_bundled_value = value;
}

BASE_EXPORT void ClearAmIBundledCache() {
  g_cached_am_i_bundled_called = false;
}

bool IsBackgroundOnlyProcess() {
  // This function really does want to examine NSBundle's idea of the main
  // bundle dictionary.  It needs to look at the actual running .app's
  // Info.plist to access its LSUIElement property.
  NSDictionary* info_dictionary = [base::mac::MainBundle() infoDictionary];
  return [info_dictionary[@"LSUIElement"] boolValue] != NO;
}

FilePath PathForFrameworkBundleResource(CFStringRef resourceName) {
  NSBundle* bundle = base::mac::FrameworkBundle();
  NSString* resourcePath = [bundle pathForResource:(NSString*)resourceName
                                            ofType:nil];
  return NSStringToFilePath(resourcePath);
}

OSType CreatorCodeForCFBundleRef(CFBundleRef bundle) {
  OSType creator = kUnknownType;
  CFBundleGetPackageInfo(bundle, NULL, &creator);
  return creator;
}

OSType CreatorCodeForApplication() {
  CFBundleRef bundle = CFBundleGetMainBundle();
  if (!bundle)
    return kUnknownType;

  return CreatorCodeForCFBundleRef(bundle);
}

bool GetSearchPathDirectory(NSSearchPathDirectory directory,
                            NSSearchPathDomainMask domain_mask,
                            FilePath* result) {
  DCHECK(result);
  NSArray<NSString*>* dirs =
      NSSearchPathForDirectoriesInDomains(directory, domain_mask, YES);
  if ([dirs count] < 1) {
    return false;
  }
  *result = NSStringToFilePath(dirs[0]);
  return true;
}

bool GetLocalDirectory(NSSearchPathDirectory directory, FilePath* result) {
  return GetSearchPathDirectory(directory, NSLocalDomainMask, result);
}

bool GetUserDirectory(NSSearchPathDirectory directory, FilePath* result) {
  return GetSearchPathDirectory(directory, NSUserDomainMask, result);
}

FilePath GetUserLibraryPath() {
  FilePath user_library_path;
  if (!GetUserDirectory(NSLibraryDirectory, &user_library_path)) {
    DLOG(WARNING) << "Could not get user library path";
  }
  return user_library_path;
}

// Takes a path to an (executable) binary and tries to provide the path to an
// application bundle containing it. It takes the outermost bundle that it can
// find (so for "/Foo/Bar.app/.../Baz.app/..." it produces "/Foo/Bar.app").
//   |exec_name| - path to the binary
//   returns - path to the application bundle, or empty on error
FilePath GetAppBundlePath(const FilePath& exec_name) {
  const char kExt[] = ".app";
  const size_t kExtLength = base::size(kExt) - 1;

  // Split the path into components.
  std::vector<std::string> components;
  exec_name.GetComponents(&components);

  // It's an error if we don't get any components.
  if (components.empty())
    return FilePath();

  // Don't prepend '/' to the first component.
  std::vector<std::string>::const_iterator it = components.begin();
  std::string bundle_name = *it;
  DCHECK_GT(it->length(), 0U);
  // If the first component ends in ".app", we're already done.
  if (it->length() > kExtLength &&
      !it->compare(it->length() - kExtLength, kExtLength, kExt, kExtLength))
    return FilePath(bundle_name);

  // The first component may be "/" or "//", etc. Only append '/' if it doesn't
  // already end in '/'.
  if (bundle_name.back() != '/')
    bundle_name += '/';

  // Go through the remaining components.
  for (++it; it != components.end(); ++it) {
    DCHECK_GT(it->length(), 0U);

    bundle_name += *it;

    // If the current component ends in ".app", we're done.
    if (it->length() > kExtLength &&
        !it->compare(it->length() - kExtLength, kExtLength, kExt, kExtLength))
      return FilePath(bundle_name);

    // Separate this component from the next one.
    bundle_name += '/';
  }

  return FilePath();
}

#define TYPE_NAME_FOR_CF_TYPE_DEFN(TypeCF) \
std::string TypeNameForCFType(TypeCF##Ref) { \
  return #TypeCF; \
}

TYPE_NAME_FOR_CF_TYPE_DEFN(CFArray)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFBag)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFBoolean)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFData)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFDate)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFDictionary)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFNull)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFNumber)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFSet)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFString)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFURL)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFUUID)

TYPE_NAME_FOR_CF_TYPE_DEFN(CGColor)

TYPE_NAME_FOR_CF_TYPE_DEFN(CTFont)
TYPE_NAME_FOR_CF_TYPE_DEFN(CTRun)

#if !defined(OS_IOS)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecCertificate)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecKey)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecPolicy)
#endif

#undef TYPE_NAME_FOR_CF_TYPE_DEFN

void NSObjectRetain(void* obj) {
  id<NSObject> nsobj = static_cast<id<NSObject> >(obj);
  [nsobj retain];
}

void NSObjectRelease(void* obj) {
  id<NSObject> nsobj = static_cast<id<NSObject> >(obj);
  [nsobj release];
}

static const char* base_bundle_id;

const char* BaseBundleID() {
  if (base_bundle_id) {
    return base_bundle_id;
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return "com.google.Chrome";
#else
  return "org.chromium.Chromium";
#endif
}

void SetBaseBundleID(const char* new_base_bundle_id) {
  if (new_base_bundle_id != base_bundle_id) {
    free((void*)base_bundle_id);
    base_bundle_id = new_base_bundle_id ? strdup(new_base_bundle_id) : NULL;
  }
}

// Definitions for the corresponding CF_TO_NS_CAST_DECL macros in
// foundation_util.h.
#define CF_TO_NS_CAST_DEFN(TypeCF, TypeNS) \
\
TypeNS* CFToNSCast(TypeCF##Ref cf_val) { \
  DCHECK(!cf_val || TypeCF##GetTypeID() == CFGetTypeID(cf_val)); \
  TypeNS* ns_val = \
      const_cast<TypeNS*>(reinterpret_cast<const TypeNS*>(cf_val)); \
  return ns_val; \
} \
\
TypeCF##Ref NSToCFCast(TypeNS* ns_val) { \
  TypeCF##Ref cf_val = reinterpret_cast<TypeCF##Ref>(ns_val); \
  DCHECK(!cf_val || TypeCF##GetTypeID() == CFGetTypeID(cf_val)); \
  return cf_val; \
}

#define CF_TO_NS_MUTABLE_CAST_DEFN(name) \
CF_TO_NS_CAST_DEFN(CF##name, NS##name) \
\
NSMutable##name* CFToNSCast(CFMutable##name##Ref cf_val) { \
  DCHECK(!cf_val || CF##name##GetTypeID() == CFGetTypeID(cf_val)); \
  NSMutable##name* ns_val = reinterpret_cast<NSMutable##name*>(cf_val); \
  return ns_val; \
} \
\
CFMutable##name##Ref NSToCFCast(NSMutable##name* ns_val) { \
  CFMutable##name##Ref cf_val = \
      reinterpret_cast<CFMutable##name##Ref>(ns_val); \
  DCHECK(!cf_val || CF##name##GetTypeID() == CFGetTypeID(cf_val)); \
  return cf_val; \
}

CF_TO_NS_MUTABLE_CAST_DEFN(Array)
CF_TO_NS_MUTABLE_CAST_DEFN(AttributedString)
CF_TO_NS_CAST_DEFN(CFCalendar, NSCalendar)
CF_TO_NS_MUTABLE_CAST_DEFN(CharacterSet)
CF_TO_NS_MUTABLE_CAST_DEFN(Data)
CF_TO_NS_CAST_DEFN(CFDate, NSDate)
CF_TO_NS_MUTABLE_CAST_DEFN(Dictionary)
CF_TO_NS_CAST_DEFN(CFError, NSError)
CF_TO_NS_CAST_DEFN(CFLocale, NSLocale)
CF_TO_NS_CAST_DEFN(CFNumber, NSNumber)
CF_TO_NS_CAST_DEFN(CFRunLoopTimer, NSTimer)
CF_TO_NS_CAST_DEFN(CFTimeZone, NSTimeZone)
CF_TO_NS_MUTABLE_CAST_DEFN(Set)
CF_TO_NS_CAST_DEFN(CFReadStream, NSInputStream)
CF_TO_NS_CAST_DEFN(CFWriteStream, NSOutputStream)
CF_TO_NS_MUTABLE_CAST_DEFN(String)
CF_TO_NS_CAST_DEFN(CFURL, NSURL)

#if defined(OS_IOS)
CF_TO_NS_CAST_DEFN(CTFont, UIFont)
#else
// The NSFont/CTFont toll-free bridging is broken before 10.15.
// http://www.openradar.me/15341349 rdar://15341349
//
// TODO(https://crbug.com/1076527): This is fixed in 10.15. When 10.15 is the
// minimum OS for Chromium, remove this specialization and replace it with just:
//
// CF_TO_NS_CAST_DEFN(CTFont, NSFont)
NSFont* CFToNSCast(CTFontRef cf_val) {
  NSFont* ns_val =
      const_cast<NSFont*>(reinterpret_cast<const NSFont*>(cf_val));
  DCHECK(!cf_val ||
         CTFontGetTypeID() == CFGetTypeID(cf_val) ||
         (_CFIsObjC(CTFontGetTypeID(), cf_val) &&
          [ns_val isKindOfClass:[NSFont class]]));
  return ns_val;
}

CTFontRef NSToCFCast(NSFont* ns_val) {
  CTFontRef cf_val = reinterpret_cast<CTFontRef>(ns_val);
  DCHECK(!cf_val ||
         CTFontGetTypeID() == CFGetTypeID(cf_val) ||
         [ns_val isKindOfClass:[NSFont class]]);
  return cf_val;
}
#endif

#undef CF_TO_NS_CAST_DEFN
#undef CF_TO_NS_MUTABLE_CAST_DEFN

#define CF_CAST_DEFN(TypeCF) \
template<> TypeCF##Ref \
CFCast<TypeCF##Ref>(const CFTypeRef& cf_val) { \
  if (cf_val == NULL) { \
    return NULL; \
  } \
  if (CFGetTypeID(cf_val) == TypeCF##GetTypeID()) { \
    return (TypeCF##Ref)(cf_val); \
  } \
  return NULL; \
} \
\
template<> TypeCF##Ref \
CFCastStrict<TypeCF##Ref>(const CFTypeRef& cf_val) { \
  TypeCF##Ref rv = CFCast<TypeCF##Ref>(cf_val); \
  DCHECK(cf_val == NULL || rv); \
  return rv; \
}

CF_CAST_DEFN(CFArray)
CF_CAST_DEFN(CFBag)
CF_CAST_DEFN(CFBoolean)
CF_CAST_DEFN(CFData)
CF_CAST_DEFN(CFDate)
CF_CAST_DEFN(CFDictionary)
CF_CAST_DEFN(CFNull)
CF_CAST_DEFN(CFNumber)
CF_CAST_DEFN(CFSet)
CF_CAST_DEFN(CFString)
CF_CAST_DEFN(CFURL)
CF_CAST_DEFN(CFUUID)

CF_CAST_DEFN(CGColor)

CF_CAST_DEFN(CTFontDescriptor)
CF_CAST_DEFN(CTRun)

#if defined(OS_IOS)
CF_CAST_DEFN(CTFont)
#else
// The NSFont/CTFont toll-free bridging is broken before 10.15.
// http://www.openradar.me/15341349 rdar://15341349
//
// TODO(https://crbug.com/1076527): This is fixed in 10.15. When 10.15 is the
// minimum OS for Chromium, remove this specialization and the #if IOS above,
// and rely just on the one CF_CAST_DEFN(CTFont).
template<> CTFontRef
CFCast<CTFontRef>(const CFTypeRef& cf_val) {
  if (cf_val == NULL) {
    return NULL;
  }
  if (CFGetTypeID(cf_val) == CTFontGetTypeID()) {
    return (CTFontRef)(cf_val);
  }

  if (!_CFIsObjC(CTFontGetTypeID(), cf_val))
    return NULL;

  id<NSObject> ns_val = reinterpret_cast<id>(const_cast<void*>(cf_val));
  if ([ns_val isKindOfClass:[NSFont class]]) {
    return (CTFontRef)(cf_val);
  }
  return NULL;
}

template<> CTFontRef
CFCastStrict<CTFontRef>(const CFTypeRef& cf_val) {
  CTFontRef rv = CFCast<CTFontRef>(cf_val);
  DCHECK(cf_val == NULL || rv);
  return rv;
}
#endif

#if !defined(OS_IOS)
CF_CAST_DEFN(SecACL)
CF_CAST_DEFN(SecCertificate)
CF_CAST_DEFN(SecKey)
CF_CAST_DEFN(SecPolicy)
CF_CAST_DEFN(SecTrustedApplication)
#endif

#undef CF_CAST_DEFN

std::string GetValueFromDictionaryErrorMessage(
    CFStringRef key, const std::string& expected_type, CFTypeRef value) {
  ScopedCFTypeRef<CFStringRef> actual_type_ref(
      CFCopyTypeIDDescription(CFGetTypeID(value)));
  return "Expected value for key " +
      base::SysCFStringRefToUTF8(key) +
      " to be " +
      expected_type +
      " but it was " +
      base::SysCFStringRefToUTF8(actual_type_ref) +
      " instead";
}

NSURL* FilePathToNSURL(const FilePath& path) {
  if (NSString* path_string = FilePathToNSString(path))
    return [NSURL fileURLWithPath:path_string];
  return nil;
}

NSString* FilePathToNSString(const FilePath& path) {
  if (path.empty())
    return nil;
  return @(path.value().c_str());  // @() does UTF8 conversion.
}

FilePath NSStringToFilePath(NSString* str) {
  if (![str length])
    return FilePath();
  return FilePath([str fileSystemRepresentation]);
}

base::ScopedCFTypeRef<CFURLRef> FilePathToCFURL(const FilePath& path) {
  DCHECK(!path.empty());

  // The function's docs promise that it does not require an NSAutoreleasePool.
  // A straightforward way to accomplish this is to use *Create* functions,
  // combined with base::ScopedCFTypeRef.
  const std::string& path_string = path.value();
  base::ScopedCFTypeRef<CFStringRef> path_cfstring(CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(path_string.data()),
      path_string.length(), kCFStringEncodingUTF8,
      /*isExternalRepresentation=*/FALSE));
  if (!path_cfstring)
    return base::ScopedCFTypeRef<CFURLRef>();

  return base::ScopedCFTypeRef<CFURLRef>(CFURLCreateWithFileSystemPath(
      kCFAllocatorDefault, path_cfstring, kCFURLPOSIXPathStyle,
      /*isDirectory=*/FALSE));
}

bool CFRangeToNSRange(CFRange range, NSRange* range_out) {
  decltype(range_out->location) end;
  if (base::IsValueInRangeForNumericType<decltype(range_out->location)>(
          range.location) &&
      base::IsValueInRangeForNumericType<decltype(range_out->length)>(
          range.length) &&
      base::CheckAdd(range.location, range.length).AssignIfValid(&end) &&
      base::IsValueInRangeForNumericType<decltype(range_out->location)>(end)) {
    *range_out = NSMakeRange(range.location, range.length);
    return true;
  }
  return false;
}

}  // namespace mac
}  // namespace base

std::ostream& operator<<(std::ostream& o, const CFStringRef string) {
  return o << base::SysCFStringRefToUTF8(string);
}

std::ostream& operator<<(std::ostream& o, const CFErrorRef err) {
  base::ScopedCFTypeRef<CFStringRef> desc(CFErrorCopyDescription(err));
  base::ScopedCFTypeRef<CFDictionaryRef> user_info(CFErrorCopyUserInfo(err));
  CFStringRef errorDesc = NULL;
  if (user_info.get()) {
    errorDesc = reinterpret_cast<CFStringRef>(
        CFDictionaryGetValue(user_info.get(), kCFErrorDescriptionKey));
  }
  o << "Code: " << CFErrorGetCode(err)
    << " Domain: " << CFErrorGetDomain(err)
    << " Desc: " << desc.get();
  if(errorDesc) {
    o << "(" << errorDesc << ")";
  }
  return o;
}

std::ostream& operator<<(std::ostream& o, CFRange range) {
  return o << NSStringFromRange(NSMakeRange(range.location, range.length));
}

std::ostream& operator<<(std::ostream& o, id obj) {
  return obj ? o << [obj description].UTF8String : o << "(nil)";
}

std::ostream& operator<<(std::ostream& o, NSRange range) {
  return o << NSStringFromRange(range);
}

std::ostream& operator<<(std::ostream& o, SEL selector) {
  return o << NSStringFromSelector(selector);
}

#if !defined(OS_IOS)
std::ostream& operator<<(std::ostream& o, NSPoint point) {
  return o << NSStringFromPoint(point);
}
std::ostream& operator<<(std::ostream& o, NSRect rect) {
  return o << NSStringFromRect(rect);
}
std::ostream& operator<<(std::ostream& o, NSSize size) {
  return o << NSStringFromSize(size);
}
#endif
