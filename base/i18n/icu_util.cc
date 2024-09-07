// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu_util.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include <string.h>

#include <memory>
#include <string>

#include "base/debug/alias.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/chromecast_buildflags.h"
#include "third_party/icu/source/common/unicode/putil.h"
#include "third_party/icu/source/common/unicode/uclean.h"
#include "third_party/icu/source/common/unicode/udata.h"
#include "third_party/icu/source/common/unicode/utrace.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/timezone_utils.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "base/ios/ios_util.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/apple/foundation_util.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/intl_profile_watcher.h"
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
#include "third_party/icu/source/common/unicode/unistr.h"
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(IS_CHROMEOS) || (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS))
#include "third_party/icu/source/i18n/unicode/timezone.h"
#endif

namespace base::i18n {

#if !BUILDFLAG(IS_NACL)
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
#if BUILDFLAG(IS_WIN)
wchar_t g_debug_icu_pf_filename[_MAX_PATH];
#endif  // BUILDFLAG(IS_WIN)
// Use an unversioned file name to simplify a icu version update down the road.
// No need to change the filename in multiple places (gyp files, windows
// build pkg configurations, etc). 'l' stands for Little Endian.
// This variable is exported through the header file.
const char kIcuDataFileName[] = "icudtl.dat";

// Time zone data loading.
// For now, only Fuchsia has a meaningful use case for this feature, so it is
// only implemented for OS_FUCHSIA.
#if BUILDFLAG(IS_FUCHSIA)
// The environment variable used to point the ICU data loader to the directory
// containing time zone data. This is available from ICU version 54. The env
// variable approach is antiquated by today's standards (2019), but is the
// recommended way to configure ICU.
//
// See for details: http://userguide.icu-project.org/datetime/timezone
const char kIcuTimeZoneEnvVariable[] = "ICU_TIMEZONE_FILES_DIR";

// Up-to-date time zone data MUST be provided by the system as a
// directory offered to Chromium components at /config/tzdata.  Chromium
// components "use" the `tzdata` directory capability, specifying the
// "/config/tzdata" path. Chromium components will crash if this capability
// is not available.
//
// TimeZoneDataTest.* tests verify that external timezone data is correctly
// loaded from the system, to alert developers if the platform and Chromium
// versions are no longer compatible versions.
// LINT.IfChange(icu_time_zone_data_path)
const char kIcuTimeZoneDataDir[] = "/config/tzdata/icu/44/le";
// LINT.ThenChange(//sandbox/policy.fuchsia/sandbox_policy_fuchsia.cc:icu_time_zone_data_path)
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_ANDROID)
const char kAndroidAssetsIcuDataFileName[] = "assets/icudtl.dat";
#endif  // BUILDFLAG(IS_ANDROID)

// File handle intentionally never closed. Not using File here because its
// Windows implementation guards against two instances owning the same
// PlatformFile (which we allow since we know it is never freed).
PlatformFile g_icudtl_pf = kInvalidPlatformFile;
MemoryMappedFile* g_icudtl_mapped_file = nullptr;
MemoryMappedFile::Region g_icudtl_region;

#if BUILDFLAG(IS_FUCHSIA)
// The directory from which the ICU data loader will be configured to load time
// zone data. It is only changed by SetIcuTimeZoneDataDirForTesting().
const char* g_icu_time_zone_data_dir = kIcuTimeZoneDataDir;
#endif  // BUILDFLAG(IS_FUCHSIA)

void LazyInitIcuDataFile() {
  if (g_icudtl_pf != kInvalidPlatformFile) {
    return;
  }
#if BUILDFLAG(IS_ANDROID)
  int fd =
      android::OpenApkAsset(kAndroidAssetsIcuDataFileName, &g_icudtl_region);
  g_icudtl_pf = fd;
  if (fd != -1) {
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  // For unit tests, data file is located on disk, so try there as a fallback.
#if !BUILDFLAG(IS_APPLE)
  FilePath data_path;
  if (!PathService::Get(DIR_ASSETS, &data_path)) {
    LOG(ERROR) << "Can't find " << kIcuDataFileName;
    return;
  }
#if BUILDFLAG(IS_WIN)
  // TODO(brucedawson): http://crbug.com/445616
  wchar_t tmp_buffer[_MAX_PATH] = {0};
  wcscpy_s(tmp_buffer, data_path.value().c_str());
  debug::Alias(tmp_buffer);
#endif
  data_path = data_path.AppendASCII(kIcuDataFileName);

#if BUILDFLAG(IS_WIN)
  // TODO(brucedawson): http://crbug.com/445616
  wchar_t tmp_buffer2[_MAX_PATH] = {0};
  wcscpy_s(tmp_buffer2, data_path.value().c_str());
  debug::Alias(tmp_buffer2);
#endif

#else  // !BUILDFLAG(IS_APPLE)
  // Assume it is in the framework bundle's Resources directory.
  FilePath data_path = apple::PathForFrameworkBundleResource(kIcuDataFileName);
#if BUILDFLAG(IS_IOS)
  FilePath override_data_path = ios::FilePathOfEmbeddedICU();
  if (!override_data_path.empty()) {
    data_path = override_data_path;
  }
#endif  // !BUILDFLAG(IS_IOS)
  if (data_path.empty()) {
    LOG(ERROR) << kIcuDataFileName << " not found in bundle";
    return;
  }
#endif  // !BUILDFLAG(IS_APPLE)
  File file(data_path, File::FLAG_OPEN | File::FLAG_READ);
  if (file.IsValid()) {
    // TODO(brucedawson): http://crbug.com/445616.
    g_debug_icu_pf_last_error = 0;
    g_debug_icu_pf_error_details = 0;
#if BUILDFLAG(IS_WIN)
    g_debug_icu_pf_filename[0] = 0;
#endif  // BUILDFLAG(IS_WIN)

    g_icudtl_pf = file.TakePlatformFile();
    g_icudtl_region = MemoryMappedFile::Region::kWholeFile;
  }
#if BUILDFLAG(IS_WIN)
  else {
    // TODO(brucedawson): http://crbug.com/445616.
    g_debug_icu_pf_last_error = ::GetLastError();
    g_debug_icu_pf_error_details = file.error_details();
    wcscpy_s(g_debug_icu_pf_filename, data_path.value().c_str());
  }
#endif  // BUILDFLAG(IS_WIN)
}

// Configures ICU to load external time zone data, if appropriate.
void InitializeExternalTimeZoneData() {
#if BUILDFLAG(IS_FUCHSIA)
  // Set the environment variable to override the location used by ICU.
  // Loading can still fail if the directory is empty or its data is invalid.
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  if (!base::DirectoryExists(base::FilePath(g_icu_time_zone_data_dir))) {
    PLOG(FATAL) << "Could not open directory: '" << g_icu_time_zone_data_dir
                << "'";
  }
  env->SetVar(kIcuTimeZoneEnvVariable, g_icu_time_zone_data_dir);
#endif  // BUILDFLAG(IS_FUCHSIA)
}

int LoadIcuData(PlatformFile data_fd,
                const MemoryMappedFile::Region& data_region,
                std::unique_ptr<MemoryMappedFile>* out_mapped_data_file,
                UErrorCode* out_error_code) {
  InitializeExternalTimeZoneData();

  if (data_fd == kInvalidPlatformFile) {
    LOG(ERROR) << "Invalid file descriptor to ICU data received.";
    return 1;  // To debug http://crbug.com/445616.
  }

  *out_mapped_data_file = std::make_unique<MemoryMappedFile>();
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
  LazyInitIcuDataFile();
  bool result =
      InitializeICUWithFileDescriptorInternal(g_icudtl_pf, g_icudtl_region);

  int debug_icu_load = g_debug_icu_load;
  debug::Alias(&debug_icu_load);
  int debug_icu_last_error = g_debug_icu_last_error;
  debug::Alias(&debug_icu_last_error);
#if BUILDFLAG(IS_WIN)
  int debug_icu_pf_last_error = g_debug_icu_pf_last_error;
  debug::Alias(&debug_icu_pf_last_error);
  int debug_icu_pf_error_details = g_debug_icu_pf_error_details;
  debug::Alias(&debug_icu_pf_error_details);
  wchar_t debug_icu_pf_filename[_MAX_PATH] = {0};
  wcscpy_s(debug_icu_pf_filename, g_debug_icu_pf_filename);
  debug::Alias(&debug_icu_pf_filename);
#endif            // BUILDFLAG(IS_WIN)
  // Excluding Chrome OS from this CHECK due to b/289684640.
#if !BUILDFLAG(IS_CHROMEOS)
  // https://crbug.com/445616
  // https://crbug.com/1449816
  CHECK(result);
#endif

  return result;
}
#endif  // (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)

// Explicitly initialize ICU's time zone if necessary.
// On some platforms, the time zone must be explicitly initialized zone rather
// than relying on ICU's internal initialization.
void InitializeIcuTimeZone() {
#if BUILDFLAG(IS_ANDROID)
  // On Android, we can't leave it up to ICU to set the default time zone
  // because ICU's time zone detection does not work in many time zones (e.g.
  // Australia/Sydney, Asia/Seoul, Europe/Paris ). Use JNI to detect the host
  // time zone and set the ICU default time zone accordingly in advance of
  // actual use. See crbug.com/722821 and
  // https://ssl.icu-project.org/trac/ticket/13208 .
  std::u16string zone_id = android::GetDefaultTimeZoneId();
  icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone(
      icu::UnicodeString(false, zone_id.data(), zone_id.length())));
#elif BUILDFLAG(IS_FUCHSIA)
  // The platform-specific mechanisms used by ICU's detectHostTimeZone() to
  // determine the default time zone will not work on Fuchsia. Therefore,
  // proactively set the default system.
  // This is also required by TimeZoneMonitorFuchsia::ProfileMayHaveChanged(),
  // which uses the current default to detect whether the time zone changed in
  // the new profile.
  // If the system time zone cannot be obtained or is not understood by ICU,
  // the "unknown" time zone will be returned by createTimeZone() and used.
  std::string zone_id =
      FuchsiaIntlProfileWatcher::GetPrimaryTimeZoneIdForIcuInitialization();
  icu::TimeZone::adoptDefault(
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(zone_id)));
#elif BUILDFLAG(IS_CHROMEOS) || (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS))
  // To respond to the time zone change properly, the default time zone
  // cache in ICU has to be populated on starting up.
  // See TimeZoneMonitorLinux::NotifyClientsFromImpl().
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
#endif  // BUILDFLAG(IS_ANDROID)
}

enum class ICUCreateInstance {
  kCharacterBreakIterator = 0,
  kWordBreakIterator = 1,
  kLineBreakIterator = 2,
  kLineBreakIteratorTypeLoose = 3,
  kLineBreakIteratorTypeNormal = 4,
  kLineBreakIteratorTypeStrict = 5,
  kSentenceBreakIterator = 6,
  kTitleBreakIterator = 7,
  kThaiBreakEngine = 8,
  kLaoBreakEngine = 9,
  kBurmeseBreakEngine = 10,
  kKhmerBreakEngine = 11,
  kChineseJapaneseBreakEngine = 12,

  kMaxValue = kChineseJapaneseBreakEngine
};

// Common initialization to run regardless of how ICU is initialized.
// There are multiple exposed InitializeIcu* functions. This should be called
// as at the end of (the last functions in the sequence of) these functions.
bool DoCommonInitialization() {
  // TODO(jungshik): Some callers do not care about tz at all. If necessary,
  // add a boolean argument to this function to init the default tz only
  // when requested.
  InitializeIcuTimeZone();

  utrace_setLevel(UTRACE_VERBOSE);
  return true;
}

}  // namespace

#if (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
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

void ResetGlobalsForTesting() {
  // Reset ICU library internal state before tearing-down the mapped data
  // file, or handle.
  u_cleanup();

  // `g_icudtl_pf` does not actually own the FD once ICU is initialized, so
  // don't try to close it here.
  g_icudtl_pf = kInvalidPlatformFile;
  delete std::exchange(g_icudtl_mapped_file, nullptr);

#if BUILDFLAG(IS_FUCHSIA)
  g_icu_time_zone_data_dir = kIcuTimeZoneDataDir;
#endif  // BUILDFLAG(IS_FUCHSIA)
}

#if BUILDFLAG(IS_FUCHSIA)
// |dir| must remain valid until ResetGlobalsForTesting() is called.
void SetIcuTimeZoneDataDirForTesting(const char* dir) {
  g_icu_time_zone_data_dir = dir;
}
#endif  // BUILDFLAG(IS_FUCHSIA)
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

#endif  // !BUILDFLAG(IS_NACL)

}  // namespace base::i18n
