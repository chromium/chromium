// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/allocator/partition_alloc_support.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/sparse_histogram.h"
#include "base/power_monitor/power_monitor_buildflags.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/trace_event/trace_log.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "build/config/compiler/compiler_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/process_memory_metrics_emitter.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/common/chrome_switches.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/metrics/android_metrics_helper.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info_values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "crypto/unexportable_key.h"
#include "crypto/unexportable_key_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "ui/base/pointer/pointer_device.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/screen.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/power_monitor/battery_state_sampler.h"
#include "chrome/browser/metrics/first_web_contents_profiler.h"
#include "chrome/browser/metrics/power/battery_discharge_reporter.h"
#include "chrome/browser/metrics/power/power_metrics_reporter.h"
#include "chrome/browser/metrics/power/process_monitor.h"
#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#if defined(__arm__)
#include <cpu-features.h>
#endif
#include "base/android/build_info.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#endif  // BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(__GLIBC__) && (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include <gnu/libc-version.h>

#include "base/linux_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/browser/metrics/key_credential_manager_support_reporter_win.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/installer/util/taskbar_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/metrics/pressure/pressure_metrics_reporter.h"
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/power_metrics/system_power_monitor.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/process_requirement.h"
#include "chrome/common/chrome_version.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

// The number of restarts to wait until removing the enable-benchmarking flag.
constexpr int kEnableBenchmarkingCountdownDefault = 3;
constexpr char kEnableBenchmarkingPrefId[] = "enable_benchmarking_countdown";

#if BUILDFLAG(IS_MAC)
constexpr char kUnexportableKeysKeychainAccessGroup[] =
    MAC_TEAM_IDENTIFIER_STRING "." MAC_BUNDLE_IDENTIFIER_STRING
                               ".unexportable-keys";
#endif  // BUILDFLAG(IS_MAC)

void RecordMemoryMetrics();

// Gets the delay for logging memory related metrics. Minimum is 1 second.
base::TimeDelta GetDelayForNextMemoryLogTest() {
  int test_delay_in_minutes;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTestMemoryLogDelayInMinutes) &&
      base::StringToInt(command_line->GetSwitchValueASCII(
                            switches::kTestMemoryLogDelayInMinutes),
                        &test_delay_in_minutes)) {
    // Setting --test-memory-log-delay-in-minutes=0 is useful for testing the
    // feature, but zero delay tends to overwhelm the system.
    return test_delay_in_minutes <= 0 ? base::Seconds(1)
                                      : base::Minutes(test_delay_in_minutes);
  }
  return memory_instrumentation::GetDelayForNextMemoryLog();
}

// Records memory metrics after a delay.
void RecordMemoryMetricsAfterDelay() {
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RecordMemoryMetrics),
      GetDelayForNextMemoryLogTest());
}

// Records memory metrics, and then triggers memory collection after a delay.
void RecordMemoryMetrics() {
  scoped_refptr<ProcessMemoryMetricsEmitter> emitter(
      new ProcessMemoryMetricsEmitter);
  emitter->FetchAndEmitProcessMemoryMetrics();

  performance_manager::PerformanceManager::RecordMemoryMetrics();

  RecordMemoryMetricsAfterDelay();
}

// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused unless the histogram is
// renamed.
enum class UmaLinuxDistro {
  kUnknown = 0,
  kOther = 1,
  kAlma = 2,
  kAlpine = 3,
  kAlter = 4,
  kAmazon = 5,
  kAnarchy = 6,
  kAntergos = 7,
  kAntiX = 8,
  kAoscOs = 9,
  kAperio = 10,
  kApricity = 11,
  kArch = 12,
  kArcoLinux = 13,
  kArtix = 14,
  kArya = 15,
  kAsteroidOs = 16,
  kBedrock = 17,
  kBitrig = 18,
  kBlackArch = 19,
  kBlag = 20,
  kBlankOn = 21,
  kBlueLight = 22,
  kBodhi = 23,
  kBonsai = 24,
  kBunsenLabs = 25,
  kCalculate = 26,
  kCarbs = 27,
  kCblMariner = 28,
  kCelOs = 29,
  kCentOs = 30,
  kChakra = 31,
  kChaletOs = 32,
  kChapeau = 33,
  kCleanjaro = 34,
  kClearLinux = 35,
  kClearOs = 36,
  kClover = 37,
  kCondres = 38,
  kContainerLinux = 39,
  kCrux = 40,
  kCrystalLinux = 41,
  kCucumber = 42,
  kCyberOs = 43,
  kDahlia = 44,
  kDarkOs = 45,
  kDebian = 46,
  kDeepin = 47,
  kDesaOs = 48,
  kDevuan = 49,
  kDracOs = 50,
  kDrauger = 51,
  kElementary = 52,
  kEndeavourOs = 53,
  kEndless = 54,
  kEuroLinux = 55,
  kExherbo = 56,
  kFedora = 57,
  kFeren = 58,
  kFrugalware = 59,
  kFuntoo = 60,
  kGalliumOs = 61,
  kGaruda = 62,
  kGentoo = 63,
  kGlaucus = 64,
  kGnewSense = 65,
  kGnome = 66,
  kGoboLinux = 67,
  kGrombyang = 68,
  kHash = 69,
  kHuayra = 70,
  kHydroOs = 71,
  kHyperbola = 72,
  kIglu = 73,
  kInstantOs = 74,
  kItc = 75,
  kJanus = 76,
  kKaOs = 77,
  kKaisen = 78,
  kKali = 79,
  kKde = 80,
  kKibojoe = 81,
  kKogaion = 82,
  kKorora = 83,
  kKsLinux = 84,
  kKubuntu = 85,
  kLangitKetujuh = 86,
  kLaxerOs = 87,
  kLede = 88,
  kLibreElec = 89,
  kLinuxLite = 90,
  kLinuxMint = 91,
  kLiveRaizo = 92,
  kLmde = 93,
  kLubuntu = 94,
  kLunar = 95,
  kMageia = 96,
  kMagpieOs = 97,
  kMandriva = 98,
  kManjaro = 99,
  kMaui = 100,
  kMer = 101,
  kMinix = 102,
  kMx = 103,
  kNamib = 104,
  kNeptune = 105,
  kNetrunner = 106,
  kNitrux = 107,
  kNixOs = 108,
  kNurunner = 109,
  kNutyX = 110,
  kObRevenge = 111,
  kObarun = 112,
  kOpenEuler = 113,
  kOpenIndiana = 114,
  kOpenMandriva = 115,
  kOpenSourceMediaCenter = 116,
  kOpenStage = 117,
  kOpenSuse = 118,
  kOpenSuseLeap = 119,
  kOpenSuseTumbleweed = 120,
  kOpenWrt = 121,
  kOpenMamba = 122,
  kOracle = 123,
  kOsElbrus = 124,
  kParabola = 125,
  kPardus = 126,
  kParrot = 127,
  kParsix = 128,
  kPcLinuxOs = 129,
  kPengwin = 130,
  kPentoo = 131,
  kPeppermint = 132,
  kPisi = 133,
  kPnmLinux = 134,
  kPopOs = 135,
  kPorteus = 136,
  kPostMarketOs = 137,
  kProxmox = 138,
  kPuffOs = 139,
  kPuppy = 140,
  kPureOs = 141,
  kQubes = 142,
  kQubyt = 143,
  kQuibian = 144,
  kRadix = 145,
  kRaspbian = 146,
  kReborn = 147,
  kRedStar = 148,
  kRedcore = 149,
  kRedhat = 150,
  kRefractedDevuan = 151,
  kRegata = 152,
  kRegolith = 153,
  kRocky = 154,
  kRosa = 155,
  kSabayon = 156,
  kSabotage = 157,
  kSailfish = 158,
  kSalentOs = 159,
  kScientific = 160,
  kSemc = 161,
  kSeptor = 162,
  kSerene = 163,
  kSharkLinux = 164,
  kSiduction = 165,
  kSkiffOs = 166,
  kSlackware = 167,
  kSliTaz = 168,
  kSmartOs = 169,
  kSolus = 170,
  kSourceMage = 171,
  kSparky = 172,
  kStar = 173,
  kSteamOs = 174,
  kSwagArch = 175,
  kT2 = 176,
  kTails = 177,
  kTeArch = 178,
  kTrisquel = 179,
  kUbuntu = 180,
  kUnivention = 181,
  kVenom = 182,
  kVnux = 183,
  kVoid = 184,
  kXferience = 185,
  kXubuntu = 186,
  kZorin = 187,

  // Needed for UMA.
  kMaxValue = kZorin,
};

enum UMALinuxGlibcVersion : uint32_t {
  UMA_LINUX_GLIBC_NOT_PARSEABLE,
  UMA_LINUX_GLIBC_UNKNOWN,
  UMA_LINUX_GLIBC_2_11,
  // To log newer versions, just update tools/metrics/histograms/histograms.xml.
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class ChromeOSChannel {
  kUnknown = 0,
  kCanary = 1,
  kDev = 2,
  kBeta = 3,
  kStable = 4,
  kMaxValue = kStable,
};

// Records the underlying Chrome OS release channel, which may be different than
// the Lacros browser's release channel.
void RecordChromeOSChannel() {
  ChromeOSChannel os_channel = ChromeOSChannel::kUnknown;
  std::string release_track;
  if (base::SysInfo::GetLsbReleaseValue(crosapi::kChromeOSReleaseTrack,
                                        &release_track)) {
    if (release_track == crosapi::kReleaseChannelStable)
      os_channel = ChromeOSChannel::kStable;
    else if (release_track == crosapi::kReleaseChannelBeta)
      os_channel = ChromeOSChannel::kBeta;
    else if (release_track == crosapi::kReleaseChannelDev)
      os_channel = ChromeOSChannel::kDev;
    else if (release_track == crosapi::kReleaseChannelCanary)
      os_channel = ChromeOSChannel::kCanary;
  }
  base::UmaHistogramEnumeration("ChromeOS.Lacros.OSChannel", os_channel);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void RecordMicroArchitectureStats() {
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  base::CPU::IntelMicroArchitecture arch = cpu.GetIntelMicroArchitecture();
  base::UmaHistogramEnumeration("Platform.IntelMaxMicroArchitecture", arch,
                                base::CPU::MAX_INTEL_MICRO_ARCHITECTURE);
#endif  // defined(ARCH_CPU_X86_FAMILY)
}

#if BUILDFLAG(IS_LINUX)
void RecordLinuxDistroSpecific(const std::string& version_string,
                               size_t parts,
                               const char* histogram_name) {
  base::Version version{version_string};
  if (!version.IsValid() || version.components().size() < parts)
    return;

  base::CheckedNumeric<int32_t> sample = 0;
  for (size_t i = 0; i < parts; i++) {
    sample *= 1000;
    sample += version.components()[i];
  }

  if (sample.IsValid())
    base::UmaHistogramSparse(histogram_name, sample.ValueOrDie());
}

// Some releases may have multiple names like "opensuse_leap", "opensuse leap",
// or "opensuseleap".  Trim non-alphanumeric characters so they all map to the
// same name.
std::string TrimLinuxDistro(const std::string& distro) {
  std::string trimmed;
  for (char c : distro) {
    if (base::IsAsciiAlphaNumeric(c)) {
      trimmed.push_back(c);
    }
  }
  return trimmed;
}

void RecordLinuxDistro() {
  std::string distro = base::GetLinuxDistro();
  if (distro.empty() || distro == "Unknown") {
    base::UmaHistogramEnumeration("Linux.Distro3", UmaLinuxDistro::kUnknown);
    return;
  }
  std::vector<std::string> distro_tokens =
      base::SplitString(distro, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  CHECK(distro_tokens.size());
  if (distro_tokens[0] == "Ubuntu") {
    // Format: Ubuntu YY.MM.P [LTS]
    // We are only concerned with release (YY.MM) not the patch (P).
    if (distro_tokens.size() >= 2) {
      RecordLinuxDistroSpecific(distro_tokens[1], 2, "Linux.Distro.Ubuntu");
    }
  } else if (distro_tokens[0] == "openSUSE") {
    // Format: openSUSE Leap RR.R
    if (distro_tokens.size() >= 3 && distro_tokens[1] == "Leap") {
      RecordLinuxDistroSpecific(distro_tokens[2], 2,
                                "Linux.Distro.OpenSuseLeap");
    }
  } else if (distro_tokens[0] == "Debian") {
    // Format: Debian GNU/Linux R.P (<codename>)
    // We are only concerned with the release (R) not the patch (P).
    if (distro_tokens.size() >= 3) {
      RecordLinuxDistroSpecific(distro_tokens[2], 1, "Linux.Distro.Debian");
    }
  } else if (distro_tokens[0] == "Fedora") {
    // Format: Fedora RR (<codename>)
    if (distro_tokens.size() >= 2) {
      RecordLinuxDistroSpecific(distro_tokens[1], 1, "Linux.Distro.Fedora");
    }
  } else if (distro_tokens.size() >= 2 && distro_tokens[1] == "Mint") {
    // Format: Linux Mint RR
    if (distro_tokens.size() >= 3) {
      RecordLinuxDistroSpecific(distro_tokens[2], 1, "Linux.Distro.Mint");
    }
  }

  using enum UmaLinuxDistro;
  // This array must be kept sorted since it is binary searched.
  constexpr std::pair<const char*, UmaLinuxDistro> kDistroPrefixes[] = {
      {"alma", kAlma},
      {"alpine", kAlpine},
      {"alter", kAlter},
      {"amazon", kAmazon},
      {"anarchy", kAnarchy},
      {"antergos", kAntergos},
      {"antix", kAntiX},
      {"aoscos", kAoscOs},
      {"aperio", kAperio},
      {"apricity", kApricity},
      {"arch", kArch},
      {"arcolinux", kArcoLinux},
      {"artix", kArtix},
      {"arya", kArya},
      {"asteroidos", kAsteroidOs},
      {"ataraxia", kJanus},
      {"bedrock", kBedrock},
      {"bitrig", kBitrig},
      {"blackarch", kBlackArch},
      {"blag", kBlag},
      {"blankon", kBlankOn},
      {"bluelight", kBlueLight},
      {"bodhi", kBodhi},
      {"bonsai", kBonsai},
      {"bunsenlabs", kBunsenLabs},
      {"calculate", kCalculate},
      {"carbs", kCarbs},
      {"cblmariner", kCblMariner},
      {"celos", kCelOs},
      {"centos", kCentOs},
      {"chakra", kChakra},
      {"chaletos", kChaletOs},
      {"chapeau", kChapeau},
      {"cleanjaro", kCleanjaro},
      {"clearlinux", kClearLinux},
      {"clearos", kClearOs},
      {"clover", kClover},
      {"condres", kCondres},
      {"containerlinux", kContainerLinux},
      {"crux", kCrux},
      {"crystallinux", kCrystalLinux},
      {"cucumber", kCucumber},
      {"cyberos", kCyberOs},
      {"dahlia", kDahlia},
      {"darkos", kDarkOs},
      {"debian", kDebian},
      {"deepin", kDeepin},
      {"desaos", kDesaOs},
      {"devuan", kDevuan},
      {"dracos", kDracOs},
      {"drauger", kDrauger},
      {"elementary", kElementary},
      {"endeavouros", kEndeavourOs},
      {"endless", kEndless},
      {"eurolinux", kEuroLinux},
      {"exherbo", kExherbo},
      {"fedora", kFedora},
      {"feren", kFeren},
      {"frugalware", kFrugalware},
      {"funtoo", kFuntoo},
      {"galliumos", kGalliumOs},
      {"garuda", kGaruda},
      {"gentoo", kGentoo},
      {"glaucus", kGlaucus},
      {"gnewsense", kGnewSense},
      {"gnome", kGnome},
      {"gobolinux", kGoboLinux},
      {"grombyang", kGrombyang},
      {"hash", kHash},
      {"huayra", kHuayra},
      {"hyperbola", kHyperbola},
      {"i3buntu", kUbuntu},
      {"iglu", kIglu},
      {"instantos", kInstantOs},
      {"itc", kItc},
      {"janus", kJanus},
      {"kaisen", kKaisen},
      {"kali", kKali},
      {"kaos", kKaOs},
      {"kde", kKde},
      {"kibojoe", kKibojoe},
      {"kogaion", kKogaion},
      {"korora", kKorora},
      {"kslinux", kKsLinux},
      {"kubuntu", kKubuntu},
      {"langitketujuh", kLangitKetujuh},
      {"laxeros", kLaxerOs},
      {"lede", kLede},
      {"libreelec", kLibreElec},
      {"linuxlite", kLinuxLite},
      {"linuxmint", kLinuxMint},
      {"liveraizo", kLiveRaizo},
      {"lmde", kLmde},
      {"lubuntu", kLubuntu},
      {"lunar", kLunar},
      {"mageia", kMageia},
      {"magpieos", kMagpieOs},
      {"mandrake", kMandriva},
      {"mandriva", kMandriva},
      {"manjaro", kManjaro},
      {"maui", kMaui},
      {"mer", kMer},
      {"minix", kMinix},
      {"mint", kLinuxMint},
      {"mx", kMx},
      {"namib", kNamib},
      {"neptune", kNeptune},
      {"netrunner", kNetrunner},
      {"nitrux", kNitrux},
      {"nixos", kNixOs},
      {"nurunner", kNurunner},
      {"nutyx", kNutyX},
      {"obarun", kObarun},
      {"obrevenge", kObRevenge},
      {"openeuler", kOpenEuler},
      {"openindiana", kOpenIndiana},
      {"openmamba", kOpenMamba},
      {"openmandriva", kOpenMandriva},
      {"opensourcemediacenter", kOpenSourceMediaCenter},
      {"openstage", kOpenStage},
      {"opensuse", kOpenSuse},
      {"opensuseleap", kOpenSuseLeap},
      {"opensusetumbleweed", kOpenSuseTumbleweed},
      {"openwrt", kOpenWrt},
      {"oracle", kOracle},
      {"oselbrus", kOsElbrus},
      {"osmc", kOpenSourceMediaCenter},
      {"parabola", kParabola},
      {"pardus", kPardus},
      {"parrot", kParrot},
      {"parsix", kParsix},
      {"pclinuxos", kPcLinuxOs},
      {"pengwin", kPengwin},
      {"pentoo", kPentoo},
      {"peppermint", kPeppermint},
      {"pisi", kPisi},
      {"pnmlinux", kPnmLinux},
      {"popos", kPopOs},
      {"porteus", kPorteus},
      {"postmarketos", kPostMarketOs},
      {"precisepuppy", kPuppy},
      {"proxmox", kProxmox},
      {"puffos", kPuffOs},
      {"puppy", kPuppy},
      {"pureos", kPureOs},
      {"qubes", kQubes},
      {"qubyt", kQubyt},
      {"quibian", kQuibian},
      {"quirkywerewolf", kPuppy},
      {"radix", kRadix},
      {"raspbian", kRaspbian},
      {"reborn", kReborn},
      {"redcore", kRedcore},
      {"redhat", kRedhat},
      {"redstar", kRedStar},
      {"refracteddevuan", kRefractedDevuan},
      {"regata", kRegata},
      {"regolith", kRegolith},
      {"rhel", kRedhat},
      {"rocky", kRocky},
      {"rosa", kRosa},
      {"sabayon", kSabayon},
      {"sabotage", kSabotage},
      {"sailfish", kSailfish},
      {"salentos", kSalentOs},
      {"scientific", kScientific},
      {"semc", kSemc},
      {"septor", kSeptor},
      {"serene", kSerene},
      {"sharklinux", kSharkLinux},
      {"siduction", kSiduction},
      {"skiffos", kSkiffOs},
      {"slackware", kSlackware},
      {"slitaz", kSliTaz},
      {"smartos", kSmartOs},
      {"solus", kSolus},
      {"sourcemage", kSourceMage},
      {"sparky", kSparky},
      {"star", kStar},
      {"steamos", kSteamOs},
      {"suse", kOpenSuse},
      {"swagarch", kSwagArch},
      {"t2", kT2},
      {"tails", kTails},
      {"tearch", kTeArch},
      {"trisquel", kTrisquel},
      {"ubuntu", kUbuntu},
      {"univention", kUnivention},
      {"venom", kVenom},
      {"vnux", kVnux},
      {"void", kVoid},
      {"whpnmlinux", kPnmLinux},
      {"xferience", kXferience},
      {"xubuntu", kXubuntu},
      {"zorin", kZorin},
  };
  struct Compare {
    bool operator()(const std::string& string,
                    const std::pair<const char*, UmaLinuxDistro>& pair) {
      return string < pair.first;
    }
  };

  std::string trimmed = TrimLinuxDistro(base::ToLowerASCII(distro));
  auto* it = std::upper_bound(kDistroPrefixes, std::end(kDistroPrefixes),
                              trimmed, Compare());
  if (it != kDistroPrefixes && base::StartsWith(trimmed, (--it)->first)) {
    base::UmaHistogramEnumeration("Linux.Distro3", it->second);
  } else {
    base::UmaHistogramEnumeration("Linux.Distro3", UmaLinuxDistro::kOther);
  }
}
#endif  // BUILDFLAG(IS_LINUX)

void RecordLinuxGlibcVersion() {
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(__GLIBC__) && (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  base::Version version(gnu_get_libc_version());

  UMALinuxGlibcVersion glibc_version_result = UMA_LINUX_GLIBC_NOT_PARSEABLE;
  if (version.IsValid() && version.components().size() == 2) {
    glibc_version_result = UMA_LINUX_GLIBC_UNKNOWN;
    uint32_t glibc_major_version = version.components()[0];
    uint32_t glibc_minor_version = version.components()[1];
    if (glibc_major_version == 2) {
      // A constant to translate glibc 2.x minor versions to their
      // equivalent UMALinuxGlibcVersion values.
      const int kGlibcMinorVersionTranslationOffset = 11 - UMA_LINUX_GLIBC_2_11;
      uint32_t translated_glibc_minor_version =
          glibc_minor_version - kGlibcMinorVersionTranslationOffset;
      if (translated_glibc_minor_version >= UMA_LINUX_GLIBC_2_11) {
        glibc_version_result =
            static_cast<UMALinuxGlibcVersion>(translated_glibc_minor_version);
      }
    }
  }
  base::UmaHistogramSparse("Linux.GlibcVersion", glibc_version_result);
#endif
}

#if BUILDFLAG(IS_WIN)
// Record the UMA histogram when a response is received.
void OnIsPinnedToTaskbarResult(bool succeeded, bool is_pinned_to_taskbar) {
  // Used for histograms; do not reorder.
  enum Result { NOT_PINNED = 0, PINNED = 1, FAILURE = 2, NUM_RESULTS };

  Result result = FAILURE;
  if (succeeded)
    result = is_pinned_to_taskbar ? PINNED : NOT_PINNED;

  base::UmaHistogramEnumeration("Windows.IsPinnedToTaskbar", result,
                                NUM_RESULTS);

  // If Chrome is not pinned to taskbar, clear the recording that the installer
  // pinned Chrome to the taskbar, so that if the user pins Chrome back to the
  // taskbar, we don't count launches as coming from an installer-pinned
  // shortcut.  TODO(crbug.com/40235395): We currently only check if
  // Chrome is pinned to the taskbar 1 out every 100 launches, which makes this
  // less meaningful, so if keeping track of whether the installer pinned Chrome
  // to the taskbar is important, we need to deal with that.

  // Record whether or not the user unpinned an installer pin of Chrome. Records
  // true if the installer pinned Chrome, and it's not pinned on this startup,
  // false if the installer pinned Chrome, and it's still pinned.
  if (GetInstallerPinnedChromeToTaskbar().value_or(false)) {
    if (result == NOT_PINNED)
      SetInstallerPinnedChromeToTaskbar(false);
    if (result != FAILURE) {
      base::UmaHistogramBoolean("Windows.InstallerPinUnpinned",
                                result == NOT_PINNED);
    }
  }
}

// Records the pinned state of the current executable into a histogram. Should
// be called on a background thread, with low priority, to avoid slowing down
// startup.
void RecordIsPinnedToTaskbarHistogram() {
  shell_integration::win::GetIsPinnedToTaskbarState(
      base::BindOnce(&OnIsPinnedToTaskbarResult));
}

// This registry key is not fully documented but there is information on it
// here:
// https://blogs.blackberry.com/en/2017/10/windows-10-parallel-loading-breakdown.
bool IsParallelDllLoadingEnabled() {
  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path))
    return false;
  const wchar_t kIFEOKey[] =
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution "
      L"Options\\";
  std::wstring browser_process_key = kIFEOKey + exe_path.BaseName().value();

  base::win::RegKey key;
  if (ERROR_SUCCESS != key.Open(HKEY_LOCAL_MACHINE, browser_process_key.c_str(),
                                KEY_QUERY_VALUE))
    return true;

  const wchar_t kMaxLoaderThreads[] = L"MaxLoaderThreads";
  DWORD max_loader_threads = 0;
  if (ERROR_SUCCESS != key.ReadValueDW(kMaxLoaderThreads, &max_loader_threads))
    return true;

  // Note: If LoaderThreads is 0, it will be set to the default value of 4.
  return max_loader_threads != 1;
}

// Records the presence (bad) or absence (good) of AcLayers.dll in the browser
// process.
void RecordAppCompatMetrics() {
  HMODULE mod = ::GetModuleHandleW(L"AcLayers.dll");
  base::UmaHistogramBoolean("Windows.AcLayersLoaded", !!mod);
}

#endif  // BUILDFLAG(IS_WIN)

void RecordDisplayHDRStatus(const display::Display& display) {
  base::UmaHistogramBoolean("Hardware.Display.SupportsHDR",
                            display.GetColorSpaces().SupportsHDR());
}

// Called on a background thread, with low priority to avoid slowing down
// startup with metrics that aren't trivial to compute.
void RecordStartupMetrics() {
#if BUILDFLAG(IS_WIN)
  const base::win::OSInfo& os_info = *base::win::OSInfo::GetInstance();
  int patch = os_info.version_number().patch;
  int build = os_info.version_number().build;
  int patch_level = 0;

  if (patch < 65536 && build < 65536)
    patch_level = MAKELONG(patch, build);
  DCHECK(patch_level) << "Windows version too high!";
  base::UmaHistogramSparse("Windows.PatchLevel", patch_level);

  int kernel32_patch = os_info.Kernel32VersionNumber().patch;
  int kernel32_build = os_info.Kernel32VersionNumber().build;
  int kernel32_patch_level = 0;
  if (kernel32_patch < 65536 && kernel32_build < 65536)
    kernel32_patch_level = MAKELONG(kernel32_patch, kernel32_build);
  DCHECK(kernel32_patch_level) << "Windows kernel32.dll version too high!";
  base::UmaHistogramSparse("Windows.PatchLevelKernel32", kernel32_patch_level);

  base::UmaHistogramBoolean("Windows.HasHighResolutionTimeTicks",
                            base::TimeTicks::IsHighResolution());
  base::UmaHistogramBoolean("Windows.HasThreadTicks",
                            base::ThreadTicks::IsSupported());

  // Determine whether parallel DLL loading is enabled for the browser process
  // executable. This is disabled by default on fresh Windows installations, but
  // the registry key that controls this might have been removed. Having the
  // parallel DLL loader enabled might affect both sandbox and early startup
  // behavior.
  base::UmaHistogramBoolean("Windows.ParallelDllLoadingEnabled",
                            IsParallelDllLoadingEnabled());
  RecordAppCompatMetrics();
  key_credential_manager_support::ReportKeyCredentialManagerSupport();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  crypto::UnexportableKeyProvider::Config config;
#if BUILDFLAG(IS_MAC)
  config.keychain_access_group = kUnexportableKeysKeychainAccessGroup;
#endif  // BUILDFLAG(IS_MAC)
  crypto::MaybeMeasureTpmOperations(std::move(config));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  // Record whether Chrome is the default browser or not.
  // Disabled on Linux due to hanging browser tests, see crbug.com/1216328.
#if !BUILDFLAG(IS_LINUX)
  shell_integration::DefaultWebClientState default_state =
      shell_integration::GetDefaultBrowser();
  base::UmaHistogramEnumeration("DefaultBrowser.State", default_state,
                                shell_integration::NUM_DEFAULT_STATES);
#endif  // !BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  RecordChromeOSChannel();
#endif

#if BUILDFLAG(IS_MAC)
  base::mac::ProcessRequirement::MaybeGatherMetrics();
#endif
}

}  // namespace

#if BUILDFLAG(IS_ANDROID)
bool IsBundleForMixedDeviceAccordingToVersionCode(
    const std::string& version_code) {
  // Primary bitness of the bundle is encoded in the last digit of the version
  // code. And the variant (package name) is encoded in the second to last.
  //
  // From build/util/android_chrome_version.py:
  //       'arm': {
  //          '32': 0,
  //          '32_64': 1,
  //          '64_32': 2,
  //          '64_32_high': 3,
  //          '64': 4,
  //      },
  //      'intel': {
  //          '32': 6,
  //          '32_64': 7,
  //          '64_32': 8,
  //          '64': 9,
  //      },
  //
  //      _PACKAGE_NAMES = {
  //          'CHROME': 0,
  //          'CHROME_MODERN': 10,
  //          'MONOCHROME': 20,
  //          'TRICHROME': 30,
  //          [...]

  if (version_code.length() < 2) {
    return false;
  }

  // '32' and '64' bundles go on 32bit-only and 64bit-only devices, so exclude
  // them.
  std::set<char> arch_codes_mixed = {'1', '2', '3', '7', '8'};
  char arch_code = version_code.back();

  // Only 'TRICHROME' supports 64-bit.
  constexpr char kTriChromeVariant = '3';
  char variant = version_code[version_code.length() - 2];

  return arch_codes_mixed.count(arch_code) > 0 && variant == kTriChromeVariant;
}
#endif  // BUILDFLAG(IS_ANDROID)

ChromeBrowserMainExtraPartsMetrics::ChromeBrowserMainExtraPartsMetrics()
    : display_count_(0) {}

ChromeBrowserMainExtraPartsMetrics::~ChromeBrowserMainExtraPartsMetrics() =
    default;

void ChromeBrowserMainExtraPartsMetrics::PreCreateThreads() {
#if !BUILDFLAG(IS_ANDROID)
  // Initialize the TabStatsTracker singleton instance. Must be initialized
  // before `responsiveness::Watcher`, which happens in
  // BrowserMainLoop::PreMainMessageLoopRun(), thus the decision to use
  // `PreCreateThreads`.
  // Only instantiate the tab stats tracker if a local state exists. This is
  // always the case for Chrome but not for the unittests.
  if (g_browser_process != nullptr &&
      g_browser_process->local_state() != nullptr) {
    metrics::TabStatsTracker::SetInstance(
        std::make_unique<metrics::TabStatsTracker>(
            g_browser_process->local_state()));
  }
#endif
}

void ChromeBrowserMainExtraPartsMetrics::PostCreateMainMessageLoop() {
#if !BUILDFLAG(IS_ANDROID)
  // Must be initialized before any child processes are spawned.
  process_monitor_ = std::make_unique<ProcessMonitor>();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeBrowserMainExtraPartsMetrics::PreProfileInit() {
  RecordMicroArchitectureStats();
}

void ChromeBrowserMainExtraPartsMetrics::PreBrowserStart() {
  flags_ui::PrefServiceFlagsStorage flags_storage(
      g_browser_process->local_state());
  about_flags::RecordUMAStatistics(&flags_storage, "Launch.FlagsAtStartup");

  // Log once here at browser start rather than at each renderer launch.
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial("ClangPGO",
#if BUILDFLAG(CLANG_PGO)
#if BUILDFLAG(USE_THIN_LTO)
                                                            "EnabledWithThinLTO"
#else
                                                            "Enabled"
#endif
#else
                                                            "Disabled"
#endif
  );

  // Records whether or not the Segment heap is in use.
#if BUILDFLAG(IS_WIN)
  if (base::win::GetVersion() >= base::win::Version::WIN10_20H1) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial("WinSegmentHeap",
#if BUILDFLAG(ENABLE_SEGMENT_HEAP)
                                                              "OptedIn"
#else
                                                              "OptedOut"
#endif
    );
  } else {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial("WinSegmentHeap",
                                                              "NotSupported");
  }
#endif  // BUILDFLAG(IS_WIN)

  // Register synthetic Finch trials proposed by PartitionAlloc.
  auto pa_trials = base::allocator::ProposeSyntheticFinchTrials();
  for (auto& trial : pa_trials) {
    auto [trial_name, group_name] = trial;
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(trial_name,
                                                              group_name);
  }

#if BUILDFLAG(IS_ANDROID)
  // Set up experiment for 64-bit Clank (incl. GWS visible IDs, so that the
  // groups are visible to Google servers).
  //
  // We are specifically interested in devices that meet all of these criteria:
  // 1) Devices with 4&6GB RAM, as we're launching the feature only for those
  //    (using (3.2;6.5) range to match RAM targeting in Play).
  // 2) Devices with only one Android profile (work versus personal), as having
  //    multiple profiles is a source of a population bias (so is having
  //    multiple users, but that bias is known to be small, and they're hard to
  //    filter out).
  // 3) Mixed 32-/64-bit devices, as non-mixed devices are forced to use
  //    a particular bitness, thus don't participate in the experiment.
  size_t ram_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  auto cpu_abi_bitness_support =
      metrics::AndroidMetricsHelper::GetInstance()->cpu_abi_bitness_support();
  bool is_device_of_interest =
      (3.2 * 1024 < ram_mb && ram_mb < 6.5 * 1024) &&
      (chrome::android::GetMultipleUserProfilesState() ==
       chrome::android::MultipleUserProfilesState::kSingleProfile) &&
      (cpu_abi_bitness_support == metrics::CpuAbiBitnessSupport::k32And64bit) &&
      IsBundleForMixedDeviceAccordingToVersionCode(
          base::android::BuildInfo::GetInstance()->package_version_code());
  if (is_device_of_interest) {
    std::vector<std::string> gws_experiment_ids;
    std::string trial_group;
    base::Version product_version(PRODUCT_VERSION);
#if defined(ARCH_CPU_64_BITS)
    trial_group = "64bit";
    gws_experiment_ids.push_back("3368915");
    if (product_version.IsValid()) {
      // For now, we only plan to run the experiment in Chrome 117+ and 118+, so
      // only send GWS IDs for those versions.
      auto milestone = product_version.components()[0];
      if (milestone >= 117) {
        gws_experiment_ids.push_back("3367345");
      }
      if (milestone >= 118) {
        gws_experiment_ids.push_back("3368917");
      }
      if (milestone >= 119) {
        gws_experiment_ids.push_back("3369945");
      }
      if (milestone >= 120) {
        gws_experiment_ids.push_back("3369947");
      }
    }
#else   // defined(ARCH_CPU_64_BITS)
    gws_experiment_ids.push_back("3368914");
    trial_group = "32bit";
    if (product_version.IsValid()) {
      // For now, we only plan to run the experiment in Chrome 117+ and 118+, so
      // only send GWS IDs for those versions.
      auto milestone = product_version.components()[0];
      if (milestone >= 117) {
        gws_experiment_ids.push_back("3367344");
      }
      if (milestone >= 118) {
        gws_experiment_ids.push_back("3368916");
      }
      if (milestone >= 119) {
        gws_experiment_ids.push_back("3369944");
      }
      if (milestone >= 120) {
        gws_experiment_ids.push_back("3369946");
      }
    }
#endif  // defined(ARCH_CPU_64_BITS)
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "BitnessForMidRangeRAM", trial_group,
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "BitnessForMidRangeRAM_wVersion",
        std::string(PRODUCT_VERSION) + "_" + trial_group,
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
    variations::VariationsIdsProvider::GetInstance()->ForceVariationIds(
        gws_experiment_ids, "");
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeBrowserMainExtraPartsMetrics::PostBrowserStart() {
  RecordMemoryMetricsAfterDelay();
  RecordLinuxGlibcVersion();

  constexpr base::TaskTraits kBestEffortTaskTraits = {
      base::MayBlock(), base::TaskPriority::BEST_EFFORT,
      base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};
#if BUILDFLAG(IS_LINUX)
  base::ThreadPool::PostTask(FROM_HERE, kBestEffortTaskTraits,
                             base::BindOnce(&RecordLinuxDistro));
#endif

#if BUILDFLAG(IS_WIN)
  // RecordStartupMetrics calls into shell_integration::GetDefaultBrowser(),
  // which requires a COM thread on Windows.
  base::ThreadPool::CreateCOMSTATaskRunner(kBestEffortTaskTraits)
      ->PostTask(FROM_HERE, base::BindOnce(&RecordStartupMetrics));
#else
  base::ThreadPool::PostTask(FROM_HERE, kBestEffortTaskTraits,
                             base::BindOnce(&RecordStartupMetrics));
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
  // TODO(isherman): The delay below is currently needed to avoid (flakily)
  // breaking some tests, including all of the ProcessMemoryMetricsEmitterTest
  // tests. Figure out why there is a dependency and fix the tests.
  auto background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(kBestEffortTaskTraits);

  // The PinnedToTaskbar histogram is CPU intensive and can trigger a crashing
  // bug in Windows or in shell extensions so just sample the data to reduce the
  // cost.
  if (base::RandGenerator(100) == 0) {
    background_task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordIsPinnedToTaskbarHistogram),
        base::Seconds(45));
  }
#endif  // BUILDFLAG(IS_WIN)

  auto* screen = display::Screen::GetScreen();
  display_count_ = screen->GetNumDisplays();
  base::UmaHistogramCounts100("Hardware.Display.Count.OnStartup",
                              display_count_);

  for (const auto& display : screen->GetAllDisplays()) {
    RecordDisplayHDRStatus(display);
  }

  display_observer_.emplace(this);

#if !BUILDFLAG(IS_ANDROID)
// In ChromeOS, the chrome application typically starts at the login screen and
// waits for the user to log in before opening a browser window, so calling
// `BeginFirstWebContentsProfiling()` is inappropriate because the
// `BrowserList` is typically empty at this point. Similarly, a restart after a
// crash (which has no login screen) requires the user to click a notification
// prompt before browser windows are restored, so the `BrowserList` is also
// empty in this case.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  metrics::BeginFirstWebContentsProfiling();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // Instantiate the power-related metrics reporters.

  // BatteryDischargeRateReporter reports the system-wide battery discharge
  // rate. It depends on the TabStatsTracker to determine the usage scenario,
  // and the BatteryStateSampler to determine the battery level.
  // The TabStatsTracker always exists (except during unit tests), while the
  // BatteryStateSampler only exists on platform where a BatteryLevelProvider
  // implementation exists.
  if (metrics::TabStatsTracker::HasInstance() &&
      base::BatteryStateSampler::Get()) {
    battery_discharge_reporter_ = std::make_unique<BatteryDischargeReporter>(
        base::BatteryStateSampler::Get());
  }

  // PowerMetricsReporter focus solely on Chrome-specific metrics that affect
  // power (CPU time, wake ups, etc.). Only instantiate it if |process_monitor_|
  // exists. This is always the case for Chrome but not for the unit tests.
  if (process_monitor_) {
    power_metrics_reporter_ =
        std::make_unique<PowerMetricsReporter>(process_monitor_.get());
  }

  if (performance_manager::features::
          ShouldUsePerformanceInterventionBackend()) {
    performance_intervention_metrics_reporter_ =
        std::make_unique<PerformanceInterventionMetricsReporter>(
            g_browser_process->local_state());
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX)
  pressure_metrics_reporter_ = std::make_unique<PressureMetricsReporter>();
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(
      power_metrics::SystemPowerMonitor::GetInstance());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  HandleEnableBenchmarkingCountdownAsync();
}

void ChromeBrowserMainExtraPartsMetrics::PreMainMessageLoopRun() {
  if (base::TimeTicks::IsConsistentAcrossProcesses()) {
    // Enable I/O jank monitoring for the browser process.
    base::EnableIOJankMonitoringForProcess(base::BindRepeating(
        [](int janky_intervals_per_minute, int total_janks_per_minute) {
          base::UmaHistogramCounts100(
              "Browser.Responsiveness.IOJankyIntervalsPerMinute",
              janky_intervals_per_minute);
          base::UmaHistogramCounts1000(
              "Browser.Responsiveness.IOJanksTotalPerMinute",
              total_janks_per_minute);
        }));
  }
}

void ChromeBrowserMainExtraPartsMetrics::PostDestroyThreads() {
#if !BUILDFLAG(IS_ANDROID)
  if (metrics::TabStatsTracker::HasInstance()) {
    // responsiveness::Watcher currently outlives TabStatsTracker and
    // RemoveObserver is never called (see UsageScenarioTracker). This should be
    // considered/addressed if refining Watcher's lifetime or migrating
    // TabStatsTracker away from global state, as this could lead to a dangling
    // pointer or similar.
    metrics::TabStatsTracker::ClearInstance();
  }

  // Reset the pointer to `performance_intervention_metrics_reporter_` to ensure
  // that PrefService outlives the metrics reporter to prevent the reporter from
  // holding a dangling pointer.
  performance_intervention_metrics_reporter_.reset();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeBrowserMainExtraPartsMetrics::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kEnableBenchmarkingPrefId,
                                kEnableBenchmarkingCountdownDefault);
}

void ChromeBrowserMainExtraPartsMetrics::HandleEnableBenchmarkingCountdown(
    PrefService* pref_service,
    std::unique_ptr<flags_ui::FlagsStorage> storage,
    flags_ui::FlagAccess access) {
  std::set<std::string> flags = storage->GetFlags();

  // The implicit assumption here is that chrome://flags are stored in
  // flags_ui::PrefServiceFlagsStorage and the multi-value switch has format
  // enable-benchmarking@<n>.
  std::string prefix =
      base::StrCat({variations::switches::kEnableBenchmarking, "@"});
  auto it = std::find_if(
      flags.begin(), flags.end(),
      [&prefix](std::string flag) { return base::StartsWith(flag, prefix); });
  if (it == flags.end()) {
    return;
  }

  int countdown = pref_service->GetInteger(kEnableBenchmarkingPrefId);
  countdown--;
  if (countdown <= 0) {
    // Clear the countdown pref.
    pref_service->ClearPref(kEnableBenchmarkingPrefId);

    // Clear the flag storage.
    flags.erase(it);
    storage->SetFlags(std::move(flags));
  } else {
    pref_service->SetInteger(kEnableBenchmarkingPrefId, countdown);
  }
}

void ChromeBrowserMainExtraPartsMetrics::
    HandleEnableBenchmarkingCountdownAsync() {
  Profile* profile = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This logic is subtle. There are two ways for ash-chrome PostBrowserStart to
  // be called on ChromeOS. The first is when the device first shows the login
  // screen. In this case the profile is the login profile. The second is after
  // the user logs in. If any flags have been changed from the login profile's
  // flags, then all of ash is restarted. We only care about invoking this logic
  // in the second case. Thus we check if IsUserLoggedIn() to guard the logic.
  if (!user_manager::UserManager::IsInitialized() ||
      !user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return;
  }
  profile = g_browser_process->profile_manager()->GetPrimaryUserProfile();
#endif
  about_flags::GetStorage(profile,
                          base::BindOnce(&HandleEnableBenchmarkingCountdown,
                                         g_browser_process->local_state()));
}

void ChromeBrowserMainExtraPartsMetrics::OnDisplayAdded(
    const display::Display& new_display) {
  EmitDisplaysChangedMetric();
  RecordDisplayHDRStatus(new_display);
}

void ChromeBrowserMainExtraPartsMetrics::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  EmitDisplaysChangedMetric();
}

void ChromeBrowserMainExtraPartsMetrics::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (changed_metrics & DisplayObserver::DISPLAY_METRIC_COLOR_SPACE) {
    RecordDisplayHDRStatus(display);
  }
}

void ChromeBrowserMainExtraPartsMetrics::EmitDisplaysChangedMetric() {
  int display_count = display::Screen::GetScreen()->GetNumDisplays();
  if (display_count != display_count_) {
    display_count_ = display_count;
    base::UmaHistogramCounts100("Hardware.Display.Count.OnChange",
                                display_count_);
  }
}

namespace chrome {

void AddMetricsExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsMetrics>());
}

}  // namespace chrome
