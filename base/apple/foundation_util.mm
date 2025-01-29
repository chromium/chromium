// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/foundation_util.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_IOS)
#import <AppKit/AppKit.h>
#endif

extern "C" {
CFTypeID SecKeyGetTypeID();
}  // extern "C"

namespace base::apple {

namespace {

bool g_cached_am_i_bundled_called = false;
bool g_cached_am_i_bundled_value = false;
bool g_override_am_i_bundled = false;
bool g_override_am_i_bundled_value = false;

bool UncachedAmIBundled() {
#if BUILDFLAG(IS_IOS)
  // All apps are bundled on iOS.
  return true;
#else
  if (g_override_am_i_bundled) {
    return g_override_am_i_bundled_value;
  }

  // Yes, this is cheap.
  return [apple::OuterBundle().bundlePath hasSuffix:@".app"];
#endif
}

bool CFURLIsFileURL(CFURLRef url) {
  ScopedCFTypeRef<CFStringRef> scheme(CFURLCopyScheme(url));
  return CFStringCompare(scheme.get(), CFSTR("file"),
                         kCFCompareCaseInsensitive) == kCFCompareEqualTo;
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
#if BUILDFLAG(IS_IOS)
  // It doesn't make sense not to be bundled on iOS.
  CHECK(value);
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
  @autoreleasepool {
    NSDictionary* info_dictionary = [apple::MainBundle() infoDictionary];
    return [info_dictionary[@"LSUIElement"] boolValue] != NO;
  }
}

FilePath PathForFrameworkBundleResource(const char* resource_name) {
  NSBundle* bundle = apple::FrameworkBundle();
  NSURL* resource_url = [bundle URLForResource:@(resource_name)
                                 withExtension:nil];
  return NSURLToFilePath(resource_url);
}

OSType CreatorCodeForCFBundleRef(CFBundleRef bundle) {
  OSType creator = kUnknownType;
  CFBundleGetPackageInfo(bundle, /*packageType=*/nullptr, &creator);
  return creator;
}

OSType CreatorCodeForApplication() {
  CFBundleRef bundle = CFBundleGetMainBundle();
  if (!bundle) {
    return kUnknownType;
  }

  return CreatorCodeForCFBundleRef(bundle);
}

bool GetSearchPathDirectory(NSSearchPathDirectory directory,
                            NSSearchPathDomainMask domain_mask,
                            FilePath* result) {
  DCHECK(result);
  NSArray<NSString*>* dirs =
      NSSearchPathForDirectoriesInDomains(directory, domain_mask, YES);
  if (dirs.count < 1) {
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

FilePath GetUserDocumentPath() {
  FilePath user_document_path;
  if (!GetUserDirectory(NSDocumentDirectory, &user_document_path)) {
    DLOG(WARNING) << "Could not get user document path";
  }
  return user_document_path;
}

// Takes a path to an (executable) binary and tries to provide the path to an
// application bundle containing it. It takes the outermost bundle that it can
// find (so for "/Foo/Bar.app/.../Baz.app/..." it produces "/Foo/Bar.app").
//   |exec_name| - path to the binary
//   returns - path to the application bundle, or empty on error
FilePath GetAppBundlePath(const FilePath& exec_name) {
  const char kExt[] = ".app";
  const size_t kExtLength = std::size(kExt) - 1;

  // Split the path into components.
  std::vector<std::string> components = exec_name.GetComponents();

  // It's an error if we don't get any components.
  if (components.empty()) {
    return FilePath();
  }

  // Don't prepend '/' to the first component.
  std::vector<std::string>::const_iterator it = components.begin();
  std::string bundle_name = *it;
  DCHECK_GT(it->length(), 0U);
  // If the first component ends in ".app", we're already done.
  if (it->length() > kExtLength &&
      !it->compare(it->length() - kExtLength, kExtLength, kExt, kExtLength)) {
    return FilePath(bundle_name);
  }

  // The first component may be "/" or "//", etc. Only append '/' if it doesn't
  // already end in '/'.
  if (bundle_name.back() != '/') {
    bundle_name += '/';
  }

  // Go through the remaining components.
  for (++it; it != components.end(); ++it) {
    DCHECK_GT(it->length(), 0U);

    bundle_name += *it;

    // If the current component ends in ".app", we're done.
    if (it->length() > kExtLength &&
        !it->compare(it->length() - kExtLength, kExtLength, kExt, kExtLength)) {
      return FilePath(bundle_name);
    }

    // Separate this component from the next one.
    bundle_name += '/';
  }

  return FilePath();
}

// Takes a path to an (executable) binary and tries to provide the path to an
// application bundle containing it. It takes the innermost bundle that it can
// find (so for "/Foo/Bar.app/.../Baz.app/..." it produces
// "/Foo/Bar.app/.../Baz.app").
//   |exec_name| - path to the binary
//   returns - path to the application bundle, or empty on error
FilePath GetInnermostAppBundlePath(const FilePath& exec_name) {
  static constexpr char kExt[] = ".app";
  static constexpr size_t kExtLength = std::size(kExt) - 1;

  // Split the path into components.
  std::vector<std::string> components = exec_name.GetComponents();

  // It's an error if we don't get any components.
  if (components.empty()) {
    return FilePath();
  }

  auto app = ranges::find_if(
      Reversed(components), [](const std::string& component) -> bool {
        return component.size() > kExtLength && EndsWith(component, kExt);
      });

  if (app == components.rend()) {
    return FilePath();
  }

  // Remove all path components after the final ".app" extension.
  components.erase(app.base(), components.end());

  std::string bundle_path;
  for (const std::string& component : components) {
    // Don't prepend a slash if this is the first component or if the
    // previous component ended with a slash, which can happen when dealing
    // with an absolute path.
    if (!bundle_path.empty() && bundle_path.back() != '/') {
      bundle_path += '/';
    }

    bundle_path += component;
  }

  return FilePath(bundle_path);
}

#define TYPE_NAME_FOR_CF_TYPE_DEFN(TypeCF)     \
  std::string TypeNameForCFType(TypeCF##Ref) { \
    return #TypeCF;                            \
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

#if !BUILDFLAG(IS_IOS)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecAccessControl)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecCertificate)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecKey)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecPolicy)
#endif

#undef TYPE_NAME_FOR_CF_TYPE_DEFN

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
    base_bundle_id = new_base_bundle_id ? strdup(new_base_bundle_id) : nullptr;
  }
}

#define CF_CAST_DEFN(TypeCF)                                       \
  template <>                                                      \
  TypeCF##Ref CFCast<TypeCF##Ref>(const CFTypeRef& cf_val) {       \
    if (cf_val == nullptr) {                                       \
      return nullptr;                                              \
    }                                                              \
    if (CFGetTypeID(cf_val) == TypeCF##GetTypeID()) {              \
      return (TypeCF##Ref)(cf_val);                                \
    }                                                              \
    return nullptr;                                                \
  }                                                                \
                                                                   \
  template <>                                                      \
  TypeCF##Ref CFCastStrict<TypeCF##Ref>(const CFTypeRef& cf_val) { \
    TypeCF##Ref rv = CFCast<TypeCF##Ref>(cf_val);                  \
    CHECK(cf_val == nullptr || rv);                                \
    return rv;                                                     \
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

CF_CAST_DEFN(CTFont)
CF_CAST_DEFN(CTFontDescriptor)
CF_CAST_DEFN(CTRun)

CF_CAST_DEFN(SecCertificate)

#if !BUILDFLAG(IS_IOS)
CF_CAST_DEFN(SecAccessControl)
CF_CAST_DEFN(SecKey)
CF_CAST_DEFN(SecPolicy)
#endif

#undef CF_CAST_DEFN

std::string GetValueFromDictionaryErrorMessage(CFStringRef key,
                                               const std::string& expected_type,
                                               CFTypeRef value) {
  ScopedCFTypeRef<CFStringRef> actual_type_ref(
      CFCopyTypeIDDescription(CFGetTypeID(value)));
  return "Expected value for key " + SysCFStringRefToUTF8(key) + " to be " +
         expected_type + " but it was " +
         SysCFStringRefToUTF8(actual_type_ref.get()) + " instead";
}

NSURL* FilePathToNSURL(const FilePath& path) {
  return apple::CFToNSOwnershipCast(FilePathToCFURL(path).release());
}

NSString* FilePathToNSString(const FilePath& path) {
  return apple::CFToNSOwnershipCast(FilePathToCFString(path).release());
}

FilePath NSStringToFilePath(NSString* str) {
  return CFStringToFilePath(apple::NSToCFPtrCast(str));
}

FilePath NSURLToFilePath(NSURL* url) {
  return CFURLToFilePath(apple::NSToCFPtrCast(url));
}

ScopedCFTypeRef<CFURLRef> FilePathToCFURL(const FilePath& path) {
  if (path.empty()) {
    return ScopedCFTypeRef<CFURLRef>();
  }

  ScopedCFTypeRef<CFStringRef> path_string(
      CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault,
                                                 path.value().c_str()));
  if (!path_string) {
    return ScopedCFTypeRef<CFURLRef>();
  }

  return ScopedCFTypeRef<CFURLRef>(CFURLCreateWithFileSystemPath(
      kCFAllocatorDefault, path_string.get(), kCFURLPOSIXPathStyle,
      /*isDirectory=*/FALSE));
}

ScopedCFTypeRef<CFStringRef> FilePathToCFString(const FilePath& path) {
  if (path.empty()) {
    return ScopedCFTypeRef<CFStringRef>();
  }

  return ScopedCFTypeRef<CFStringRef>(
      CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault,
                                                 path.value().c_str()));
}

FilePath CFStringToFilePath(CFStringRef str) {
  if (!str || CFStringGetLength(str) == 0) {
    return FilePath();
  }

  return FilePath(FilePath::GetHFSDecomposedForm(str));
}

FilePath CFURLToFilePath(CFURLRef url) {
  if (!url || !CFURLIsFileURL(url)) {
    return FilePath();
  }

  ScopedCFTypeRef<CFStringRef> path(
      CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle));
  if (!path) {
    return FilePath();
  }

  return CFStringToFilePath(path.get());
}

bool CFRangeToNSRange(CFRange range, NSRange* range_out) {
  NSUInteger end;
  if (IsValueInRangeForNumericType<NSUInteger>(range.location) &&
      IsValueInRangeForNumericType<NSUInteger>(range.length) &&
      CheckAdd(range.location, range.length).AssignIfValid(&end) &&
      IsValueInRangeForNumericType<NSUInteger>(end)) {
    *range_out = NSMakeRange(static_cast<NSUInteger>(range.location),
                             static_cast<NSUInteger>(range.length));
    return true;
  }
  return false;
}

span<const uint8_t> CFDataToSpan(CFDataRef data) {
  return NSDataToSpan(apple::CFToNSPtrCast(data));
}

span<uint8_t> CFMutableDataToSpan(CFMutableDataRef data) {
  return NSMutableDataToSpan(apple::CFToNSPtrCast(data));
}

}  // namespace base::apple

std::ostream& operator<<(std::ostream& o, const CFStringRef string) {
  return o << base::SysCFStringRefToUTF8(string);
}

std::ostream& operator<<(std::ostream& o, const CFErrorRef err) {
  base::apple::ScopedCFTypeRef<CFStringRef> desc(CFErrorCopyDescription(err));
  base::apple::ScopedCFTypeRef<CFDictionaryRef> user_info(
      CFErrorCopyUserInfo(err));
  CFStringRef errorDesc = nullptr;
  if (user_info.get()) {
    errorDesc = reinterpret_cast<CFStringRef>(
        CFDictionaryGetValue(user_info.get(), kCFErrorDescriptionKey));
  }
  o << "Code: " << CFErrorGetCode(err) << " Domain: " << CFErrorGetDomain(err)
    << " Desc: " << desc.get();
  if (errorDesc) {
    o << "(" << errorDesc << ")";
  }
  return o;
}

std::ostream& operator<<(std::ostream& o, CFRange range) {
  return o << NSStringFromRange(
             NSMakeRange(static_cast<NSUInteger>(range.location),
                         static_cast<NSUInteger>(range.length)));
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

#if !BUILDFLAG(IS_IOS)
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
