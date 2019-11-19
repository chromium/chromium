// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu_util.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "third_party/icu/source/common/unicode/putil.h"
#include "third_party/icu/source/common/unicode/udata.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/timezone_utils.h"
#endif

#if defined(OS_IOS)
#include "base/ios/ios_util.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif

#if defined(OS_ANDROID) || (defined(OS_LINUX) && !defined(IS_CHROMECAST))
#include "third_party/icu/source/i18n/unicode/timezone.h"
#endif

namespace base {
namespace i18n {

#if !defined(OS_NACL)
namespace {

#if DCHECK_IS_ON()
// Assert that we are not called more than once.  Even though calling this
// function isn't harmful (ICU can handle it), being called twice probably
// indicates a programming error.
bool g_check_called_once = true;
bool g_called_once = false;
#endif  // DCHECK_IS_ON()

#if (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)

// To debug http://crbug.com/445616.
int g_debug_icu_last_error;
int g_debug_icu_load;
int g_debug_icu_pf_error_details;
int g_debug_icu_pf_last_error;
#if defined(OS_WIN)
wchar_t g_debug_icu_pf_filename[_MAX_PATH];
#endif  // OS_WIN
// Use an unversioned file name to simplify a icu version update down the road.
// No need to change the filename in multiple places (gyp files, windows
// build pkg configurations, etc). 'l' stands for Little Endian.
// This variable is exported through the header file.
const char kIcuDataFileName[] = "icudtl.dat";
const char kIcuExtraDataFileName[] = "icudtl_extra.dat";

#if defined(OS_ANDROID)
const char kAssetsPathPrefix[] = "assets/";
#endif  // defined(OS_ANDROID)

// File handle intentionally never closed. Not using File here because its
// Windows implementation guards against two instances owning the same
// PlatformFile (which we allow since we know it is never freed).
PlatformFile g_icudtl_pf = kInvalidPlatformFile;
MemoryMappedFile* g_icudtl_mapped_file = nullptr;
MemoryMappedFile::Region g_icudtl_region;
PlatformFile g_icudtl_extra_pf = kInvalidPlatformFile;
MemoryMappedFile* g_icudtl_extra_mapped_file = nullptr;
MemoryMappedFile::Region g_icudtl_extra_region;

struct PfRegion {
 public:
  PlatformFile pf;
  MemoryMappedFile::Region region;
};

std::unique_ptr<PfRegion> OpenIcuDataFile(const std::string& filename) {
  auto result = std::make_unique<PfRegion>();
#if defined(OS_ANDROID)
  result->pf =
      android::OpenApkAsset(kAssetsPathPrefix + filename, &result->region);
  if (result->pf != -1) {
    return result;
  }
#endif  // defined(OS_ANDROID)
  // For unit tests, data file is located on disk, so try there as a fallback.
#if !defined(OS_MACOSX)
  FilePath data_path;
  if (!PathService::Get(DIR_ASSETS, &data_path)) {
    LOG(ERROR) << "Can't find " << filename;
    return nullptr;
  }
#if defined(OS_WIN)
  // TODO(brucedawson): http://crbug.com/445616
  wchar_t tmp_buffer[_MAX_PATH] = {0};
  wcscpy_s(tmp_buffer, as_wcstr(data_path.value()));
  debug::Alias(tmp_buffer);
#endif
  data_path = data_path.AppendASCII(filename);

#if defined(OS_WIN)
  // TODO(brucedawson): http://crbug.com/445616
  wchar_t tmp_buffer2[_MAX_PATH] = {0};
  wcscpy_s(tmp_buffer2, as_wcstr(data_path.value()));
  debug::Alias(tmp_buffer2);
#endif

#else  // !defined(OS_MACOSX)
  // Assume it is in the framework bundle's Resources directory.
  ScopedCFTypeRef<CFStringRef> data_file_name(SysUTF8ToCFStringRef(filename));
  FilePath data_path = mac::PathForFrameworkBundleResource(data_file_name);
#if defined(OS_IOS)
  FilePath override_data_path = ios::FilePathOfEmbeddedICU();
  if (!override_data_path.empty()) {
    data_path = override_data_path;
  }
#endif  // !defined(OS_IOS)
  if (data_path.empty()) {
    LOG(ERROR) << filename << " not found in bundle";
    return nullptr;
  }
#endif  // !defined(OS_MACOSX)
  File file(data_path, File::FLAG_OPEN | File::FLAG_READ);
  if (file.IsValid()) {
    // TODO(brucedawson): http://crbug.com/445616.
    g_debug_icu_pf_last_error = 0;
    g_debug_icu_pf_error_details = 0;
#if defined(OS_WIN)
    g_debug_icu_pf_filename[0] = 0;
#endif  // OS_WIN

    result->pf = file.TakePlatformFile();
    result->region = MemoryMappedFile::Region::kWholeFile;
  }
#if defined(OS_WIN)
  else {
    // TODO(brucedawson): http://crbug.com/445616.
    g_debug_icu_pf_last_error = ::GetLastError();
    g_debug_icu_pf_error_details = file.error_details();
    wcscpy_s(g_debug_icu_pf_filename, as_wcstr(data_path.value()));
  }
#endif  // OS_WIN

  return result;
}

void LazyOpenIcuDataFile() {
  if (g_icudtl_pf != kInvalidPlatformFile) {
    return;
  }
  auto pf_region = OpenIcuDataFile(kIcuDataFileName);
  if (!pf_region) {
    return;
  }
  g_icudtl_pf = pf_region->pf;
  g_icudtl_region = pf_region->region;
}

int LoadIcuData(PlatformFile data_fd,
                const MemoryMappedFile::Region& data_region,
                std::unique_ptr<MemoryMappedFile>* out_mapped_data_file,
                UErrorCode* out_error_code) {
  if (data_fd == kInvalidPlatformFile) {
    LOG(ERROR) << "Invalid file descriptor to ICU data received.";
    return 1;  // To debug http://crbug.com/445616.
  }

  out_mapped_data_file->reset(new MemoryMappedFile());
  if (!(*out_mapped_data_file)->Initialize(File(data_fd), data_region)) {
    LOG(ERROR) << "Couldn't mmap icu data file";
    return 2;  // To debug http://crbug.com/445616.
  }

  (*out_error_code) = U_ZERO_ERROR;
  udata_setCommonData(const_cast<uint8_t*>((*out_mapped_data_file)->data()),
                      out_error_code);
  if (U_FAILURE(*out_error_code)) {
    LOG(ERROR) << "Failed to initialize ICU with data file: "
               << u_errorName(*out_error_code);
    return 3;  // To debug http://crbug.com/445616.
  }

  return 0;
}

bool InitializeICUWithFileDescriptorInternal(
    PlatformFile data_fd,
    const MemoryMappedFile::Region& data_region) {
  // This can be called multiple times in tests.
  if (g_icudtl_mapped_file) {
    g_debug_icu_load = 0;  // To debug http://crbug.com/445616.
    return true;
  }

  std::unique_ptr<MemoryMappedFile> mapped_file;
  UErrorCode err;
  g_debug_icu_load = LoadIcuData(data_fd, data_region, &mapped_file, &err);
  if (g_debug_icu_load == 1 || g_debug_icu_load == 2) {
    return false;
  }
  g_icudtl_mapped_file = mapped_file.release();

  if (g_debug_icu_load == 3) {
    g_debug_icu_last_error = err;
  }

  // Never try to load ICU data from files.
  udata_setFileAccess(UDATA_ONLY_PACKAGES, &err);
  return U_SUCCESS(err);
}

bool InitializeICUFromDataFile() {
  // If the ICU data directory is set, ICU won't actually load the data until
  // it is needed.  This can fail if the process is sandboxed at that time.
  // Instead, we map the file in and hand off the data so the sandbox won't
  // cause any problems.
  LazyOpenIcuDataFile();
  bool result =
      InitializeICUWithFileDescriptorInternal(g_icudtl_pf, g_icudtl_region);

#if defined(OS_WIN)
  int debug_icu_load = g_debug_icu_load;
  debug::Alias(&debug_icu_load);
  int debug_icu_last_error = g_debug_icu_last_error;
  debug::Alias(&debug_icu_last_error);
  int debug_icu_pf_last_error = g_debug_icu_pf_last_error;
  debug::Alias(&debug_icu_pf_last_error);
  int debug_icu_pf_error_details = g_debug_icu_pf_error_details;
  debug::Alias(&debug_icu_pf_error_details);
  wchar_t debug_icu_pf_filename[_MAX_PATH] = {0};
  wcscpy_s(debug_icu_pf_filename, g_debug_icu_pf_filename);
  debug::Alias(&debug_icu_pf_filename);
  CHECK(result);  // TODO(brucedawson): http://crbug.com/445616
#endif            // defined(OS_WIN)

  return result;
}
#endif  // (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)

// Explicitly initialize ICU's time zone if necessary.
// On some platforms, the time zone must be explicitly initialized zone rather
// than relying on ICU's internal initialization.
void InitializeIcuTimeZone() {
#if defined(OS_ANDROID)
  // On Android, we can't leave it up to ICU to set the default timezone
  // because ICU's timezone detection does not work in many timezones (e.g.
  // Australia/Sydney, Asia/Seoul, Europe/Paris ). Use JNI to detect the host
  // timezone and set the ICU default timezone accordingly in advance of
  // actual use. See crbug.com/722821 and
  // https://ssl.icu-project.org/trac/ticket/13208 .
  string16 zone_id = android::GetDefaultTimeZoneId();
  icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone(
      icu::UnicodeString(FALSE, zone_id.data(), zone_id.length())));
#elif defined(OS_LINUX) && !defined(IS_CHROMECAST)
  // To respond to the timezone change properly, the default timezone
  // cache in ICU has to be populated on starting up.
  // See TimeZoneMonitorLinux::NotifyClientsFromImpl().
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
#endif  // defined(OS_ANDROID)
}

// Common initialization to run regardless of how ICU is initialized.
// There are multiple exposed InitializeIcu* functions. This should be called
// as at the end of (the last functions in the sequence of) these functions.
bool DoCommonInitialization() {
  // TODO(jungshik): Some callers do not care about tz at all. If necessary,
  // add a boolean argument to this function to init the default tz only
  // when requested.
  InitializeIcuTimeZone();

  return true;
}

}  // namespace

#if (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
bool InitializeExtraICUWithFileDescriptor(
    PlatformFile data_fd,
    const MemoryMappedFile::Region& data_region) {
  if (g_icudtl_pf != kInvalidPlatformFile) {
    // Must call InitializeExtraICUWithFileDescriptor() before
    // InitializeICUWithFileDescriptor().
    return false;
  }
  std::unique_ptr<MemoryMappedFile> mapped_file;
  UErrorCode err;
  if (LoadIcuData(data_fd, data_region, &mapped_file, &err) != 0) {
    return false;
  }
  g_icudtl_extra_mapped_file = mapped_file.release();
  return true;
}

bool InitializeICUWithFileDescriptor(
    PlatformFile data_fd,
    const MemoryMappedFile::Region& data_region) {
#if DCHECK_IS_ON()
  DCHECK(!g_check_called_once || !g_called_once);
  g_called_once = true;
#endif
  if (!InitializeICUWithFileDescriptorInternal(data_fd, data_region))
    return false;

  return DoCommonInitialization();
}

PlatformFile GetIcuDataFileHandle(MemoryMappedFile::Region* out_region) {
  CHECK_NE(g_icudtl_pf, kInvalidPlatformFile);
  *out_region = g_icudtl_region;
  return g_icudtl_pf;
}

PlatformFile GetIcuExtraDataFileHandle(MemoryMappedFile::Region* out_region) {
  if (g_icudtl_extra_pf == kInvalidPlatformFile) {
    return kInvalidPlatformFile;
  }
  *out_region = g_icudtl_extra_region;
  return g_icudtl_extra_pf;
}

bool InitializeExtraICU() {
  if (g_icudtl_pf != kInvalidPlatformFile) {
    // Must call InitializeExtraICU() before InitializeICU().
    return false;
  }
  auto pf_region = OpenIcuDataFile(kIcuExtraDataFileName);
  if (!pf_region) {
    return false;
  }
  g_icudtl_extra_pf = pf_region->pf;
  g_icudtl_extra_region = pf_region->region;
  std::unique_ptr<MemoryMappedFile> mapped_file;
  UErrorCode err;
  if (LoadIcuData(g_icudtl_extra_pf, g_icudtl_extra_region, &mapped_file,
                  &err) != 0) {
    return false;
  }
  g_icudtl_extra_mapped_file = mapped_file.release();
  return true;
}

void ResetGlobalsForTesting() {
  g_icudtl_pf = kInvalidPlatformFile;
  g_icudtl_mapped_file = nullptr;
  g_icudtl_extra_pf = kInvalidPlatformFile;
  g_icudtl_extra_mapped_file = nullptr;
}
#endif  // (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)

bool InitializeICU() {
#if DCHECK_IS_ON()
  DCHECK(!g_check_called_once || !g_called_once);
  g_called_once = true;
#endif

#if (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_STATIC)
  // The ICU data is statically linked.
#elif (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
  if (!InitializeICUFromDataFile())
    return false;
#else
#error Unsupported ICU_UTIL_DATA_IMPL value
#endif  // (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_STATIC)

  return DoCommonInitialization();
}

void AllowMultipleInitializeCallsForTesting() {
#if DCHECK_IS_ON()
  g_check_called_once = false;
#endif
}

#endif  // !defined(OS_NACL)

}  // namespace i18n
}  // namespace base
