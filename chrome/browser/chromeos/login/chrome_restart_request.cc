// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/chrome_restart_request.h"

#include <sys/socket.h>
#include <vector>

#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/user_manager/user_names.h"
#include "components/viz/common/switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/media_switches.h"
#include "media/capture/capture_switches.h"
#include "media/media_buildflags.h"
#include "sandbox/policy/switches.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_features.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/wm/core/wm_core_switches.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Increase logging level for Guest mode to avoid INFO messages in logs.
const char kGuestModeLoggingLevel[] = "1";

// Derives the new command line from |base_command_line| by doing the following:
// - Forward a given switches list to new command;
// - Set start url if given;
// - Append/override switches using |new_switches|;
void DeriveCommandLine(const GURL& start_url,
                       const base::CommandLine& base_command_line,
                       const base::DictionaryValue& new_switches,
                       base::CommandLine* command_line) {
  DCHECK_NE(&base_command_line, command_line);

  static const char* const kForwardSwitches[] = {
    sandbox::policy::switches::kDisableGpuSandbox,
    sandbox::policy::switches::kDisableSeccompFilterSandbox,
    sandbox::policy::switches::kDisableSetuidSandbox,
    sandbox::policy::switches::kGpuSandboxAllowSysVShm,
    sandbox::policy::switches::kGpuSandboxFailuresFatal,
    sandbox::policy::switches::kNoSandbox,
    ::switches::kDisable2dCanvasImageChromium,
    ::switches::kDisableAccelerated2dCanvas,
    ::switches::kDisableAcceleratedMjpegDecode,
    ::switches::kDisableAcceleratedVideoDecode,
    ::switches::kDisableAcceleratedVideoEncode,
    ::switches::kDisableBlinkFeatures,
    ::switches::kDisableGpu,
    ::switches::kDisableGpuMemoryBufferVideoFrames,
    ::switches::kDisableGpuShaderDiskCache,
    ::switches::kUseCmdDecoder,
    ::switches::kUseANGLE,
    ::switches::kDisableGpuWatchdog,
    ::switches::kDisableGpuCompositing,
    ::switches::kDisableGpuRasterization,
    ::switches::kDisableOopRasterization,
    ::switches::kDisablePepper3DImageChromium,
    ::switches::kDisableTouchDragDrop,
    ::switches::kDisableVideoCaptureUseGpuMemoryBuffer,
    ::switches::kDisableYUVImageDecoding,
    ::switches::kEnableBlinkFeatures,
    ::switches::kEnableGpuMemoryBufferVideoFrames,
    ::switches::kEnableGpuRasterization,
    ::switches::kEnableLogging,
    ::switches::kEnableNativeGpuMemoryBuffers,
    ::switches::kEnableOopRasterization,
    ::switches::kEnableTouchDragDrop,
    ::switches::kEnableUnifiedDesktop,
    ::switches::kEnableUseZoomForDSF,
    ::switches::kEnableViewport,
    ::switches::kEnableHardwareOverlays,
    ::switches::kEdgeTouchFiltering,
    ::switches::kHostWindowBounds,
    ::switches::kMainFrameResizesAreOrientationChanges,
    ::switches::kForceDeviceScaleFactor,
    ::switches::kForceGpuMemAvailableMb,
    ::switches::kGpuStartupDialog,
    ::switches::kGpuSandboxStartEarly,
    ::switches::kNumRasterThreads,
    ::switches::kPlatformDisallowsChromeOSDirectVideoDecoder,
    ::switches::kPpapiFlashArgs,
    ::switches::kPpapiFlashPath,
    ::switches::kPpapiFlashVersion,
    ::switches::kPpapiInProcess,
    ::switches::kRemoteDebuggingPort,
    ::switches::kRendererStartupDialog,
    ::switches::kSchedulerConfigurationDefault,
    ::switches::kTouchDevices,
    ::switches::kTouchEventFeatureDetection,
    ::switches::kTopChromeTouchUi,
    ::switches::kTraceToConsole,
    ::switches::kUIDisablePartialSwap,
#if defined(USE_CRAS)
    ::switches::kUseCras,
#endif
    ::switches::kUseGL,
    ::switches::kUserDataDir,
    ::switches::kV,
    ::switches::kVModule,
    ::switches::kVideoCaptureUseGpuMemoryBuffer,
    ::switches::kEnableWebGLDraftExtensions,
    ::switches::kDisableWebGLImageChromium,
    ::switches::kEnableWebGLImageChromium,
    ::switches::kEnableUnsafeWebGPU,
    ::switches::kDisableWebRtcHWDecoding,
    ::switches::kDisableWebRtcHWEncoding,
    ::switches::kOzonePlatform,
    ash::switches::kAshClearFastInkBuffer,
    ash::switches::kAshEnableTabletMode,
    ash::switches::kAshEnableWaylandServer,
    ash::switches::kAshForceEnableStylusTools,
    ash::switches::kAshEnablePaletteOnAllDisplays,
    ash::switches::kAshTouchHud,
    ash::switches::kAuraLegacyPowerButton,
    ash::switches::kEnableDimShelf,
    ash::switches::kShowTaps,
    blink::switches::kBlinkSettings,
    blink::switches::kDarkModeSettings,
    blink::switches::kDisableLowResTiling,
    blink::switches::kDisablePartialRaster,
    blink::switches::kDisablePreferCompositingToLCDText,
    blink::switches::kDisableRGBA4444Textures,
    blink::switches::kDisableThreadedScrolling,
    blink::switches::kDisableZeroCopy,
    blink::switches::kEnableLowResTiling,
    blink::switches::kEnablePreferCompositingToLCDText,
    blink::switches::kEnableRGBA4444Textures,
    blink::switches::kEnableZeroCopy,
    blink::switches::kGpuRasterizationMSAASampleCount,
    chromeos::switches::kDefaultWallpaperLarge,
    chromeos::switches::kDefaultWallpaperSmall,
    chromeos::switches::kGuestWallpaperLarge,
    chromeos::switches::kGuestWallpaperSmall,
    // Please keep these in alphabetical order. Non-UI Compositor switches
    // here should also be added to
    // content/browser/renderer_host/render_process_host_impl.cc.
    cc::switches::kCheckDamageEarly,
    cc::switches::kDisableCompositedAntialiasing,
    cc::switches::kDisableMainFrameBeforeActivation,
    cc::switches::kDisableThreadedAnimation,
    cc::switches::kEnableGpuBenchmarking,
    cc::switches::kEnableMainFrameBeforeActivation,
    cc::switches::kHighlightNonLCDTextLayers,
    cc::switches::kShowCompositedLayerBorders,
    cc::switches::kShowFPSCounter,
    cc::switches::kShowLayerAnimationBounds,
    cc::switches::kShowPropertyChangedRects,
    cc::switches::kShowScreenSpaceRects,
    cc::switches::kShowSurfaceDamageRects,
    cc::switches::kSlowDownRasterScaleFactor,
    cc::switches::kUIEnableLayerLists,
    cc::switches::kUIShowFPSCounter,
    chromeos::switches::kArcAvailability,
    chromeos::switches::kArcAvailable,
    chromeos::switches::kArcScale,
    chromeos::switches::kDbusStub,
    chromeos::switches::kDisableArcDataWipe,
    chromeos::switches::kDisableArcOptInVerification,
    chromeos::switches::kDisableLoginAnimations,
    chromeos::switches::kEnableArc,
    chromeos::switches::kEnterpriseDisableArc,
    chromeos::switches::kEnterpriseEnableForcedReEnrollment,
    chromeos::switches::kHasChromeOSKeyboard,
    chromeos::switches::kLoginProfile,
    chromeos::switches::kNaturalScrollDefault,
    chromeos::switches::kRlzPingDelay,
    chromeos::switches::kSystemInDevMode,
    policy::switches::kDeviceManagementUrl,
    wm::switches::kWindowAnimationsDisabled,
  };
  command_line->CopySwitchesFrom(base_command_line, kForwardSwitches,
                                 base::size(kForwardSwitches));

  if (start_url.is_valid())
    command_line->AppendArg(start_url.spec());

  for (base::DictionaryValue::Iterator it(new_switches); !it.IsAtEnd();
       it.Advance()) {
    std::string value;
    CHECK(it.value().GetAsString(&value));
    command_line->AppendSwitchASCII(it.key(), value);
  }
}

// Adds allowlisted features to |out_command_line| if they are enabled in the
// current session.
void DeriveEnabledFeatures(base::CommandLine* out_command_line) {
  static const base::Feature* kForwardEnabledFeatures[] = {
      &ash::features::kAutoNightLight,
  };

  std::vector<std::string> enabled_features;
  for (const auto* feature : kForwardEnabledFeatures) {
    if (base::FeatureList::IsEnabled(*feature))
      enabled_features.push_back(feature->name);
  }

  if (enabled_features.empty())
    return;

  out_command_line->AppendSwitchASCII("enable-features",
                                      base::JoinString(enabled_features, ","));
}

// Simulates a session manager restart by launching give command line
// and exit current process.
void ReLaunch(const base::CommandLine& command_line) {
  base::LaunchProcess(command_line.argv(), base::LaunchOptions());
  chrome::AttemptUserExit();
}

// Wraps the work of sending chrome restart request to session manager.
// If local state is present, try to commit it first. The request is fired when
// the commit goes through or some time (3 seconds) has elapsed.
class ChromeRestartRequest
    : public base::SupportsWeakPtr<ChromeRestartRequest> {
 public:
  explicit ChromeRestartRequest(const std::vector<std::string>& argv);
  ~ChromeRestartRequest();

  // Starts the request.
  void Start();

 private:
  // Fires job restart request to session manager.
  void RestartJob();

  // Called when RestartJob D-Bus method call is complete.
  void OnRestartJob(base::ScopedFD local_auth_fd, bool result);

  const std::vector<std::string> argv_;
  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRestartRequest);
};

ChromeRestartRequest::ChromeRestartRequest(const std::vector<std::string>& argv)
    : argv_(argv) {}

ChromeRestartRequest::~ChromeRestartRequest() {}

void ChromeRestartRequest::Start() {
  VLOG(1) << "Requesting a restart with command line: "
          << base::JoinString(argv_, " ");

  // Session Manager may kill the chrome anytime after this point.
  // Write exit_cleanly and other stuff to the disk here.
  g_browser_process->EndSession();

  // XXX: normally this call must not be needed, however RestartJob
  // just kills us so settings may be lost. See http://crosbug.com/13102
  g_browser_process->FlushLocalStateAndReply(
      base::BindOnce(&ChromeRestartRequest::RestartJob, AsWeakPtr()));
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(3), this,
               &ChromeRestartRequest::RestartJob);
}

void ChromeRestartRequest::RestartJob() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  VLOG(1) << "ChromeRestartRequest::RestartJob";

  // The session manager requires a RestartJob caller to open a socket pair and
  // pass one end over D-Bus while holding the local end open for the duration
  // of the call.
  int sockets[2] = {-1, -1};
  // socketpair() doesn't cause disk IO so it's OK to call it on the UI thread.
  // Also, the current chrome process is going to die soon so it doesn't matter
  // anyways.
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
    PLOG(ERROR) << "Failed to create a unix domain socketpair";
    delete this;
    return;
  }
  base::ScopedFD local_auth_fd(sockets[0]);
  base::ScopedFD remote_auth_fd(sockets[1]);
  // Ownership of local_auth_fd is passed to the callback that is to be
  // called on completion of this method call. This keeps the browser end
  // of the socket-pair alive for the duration of the RPC.
  SessionManagerClient::Get()->RestartJob(
      remote_auth_fd.get(), argv_,
      base::BindOnce(&ChromeRestartRequest::OnRestartJob, AsWeakPtr(),
                     base::Passed(&local_auth_fd)));
}

void ChromeRestartRequest::OnRestartJob(base::ScopedFD local_auth_fd,
                                        bool result) {
  // Now that the call is complete, local_auth_fd can be closed and discarded,
  // which will happen automatically when it goes out of scope.
  VLOG(1) << "OnRestartJob";
  delete this;
}

}  // namespace

void GetOffTheRecordCommandLine(const GURL& start_url,
                                bool is_oobe_completed,
                                const base::CommandLine& base_command_line,
                                base::CommandLine* command_line) {
  base::DictionaryValue otr_switches;
  otr_switches.SetString(switches::kGuestSession, std::string());
  otr_switches.SetString(::switches::kIncognito, std::string());
  otr_switches.SetString(::switches::kLoggingLevel, kGuestModeLoggingLevel);
  otr_switches.SetString(
      switches::kLoginUser,
      cryptohome::Identification(user_manager::GuestAccountId()).id());

  // Override the home page.
  otr_switches.SetString(::switches::kHomePage,
                         GURL(chrome::kChromeUINewTabURL).spec());

  // If OOBE is not finished yet, lock down the guest session to not allow
  // surfing the web. Guest mode is still useful to inspect logs and run network
  // diagnostics.
  if (!is_oobe_completed)
    otr_switches.SetString(switches::kOobeGuestSession, std::string());

  DeriveCommandLine(start_url, base_command_line, otr_switches, command_line);
  DeriveEnabledFeatures(command_line);
}

void RestartChrome(const base::CommandLine& command_line) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BootTimesRecorder::Get()->set_restart_requested();

  static bool restart_requested = false;
  if (restart_requested) {
    NOTREACHED() << "Request chrome restart for more than once.";
  }
  restart_requested = true;

  if (!SessionManagerClient::Get()->SupportsBrowserRestart()) {
    // Do nothing when running as test on bots or a dev box.
    const base::CommandLine* current_command_line =
        base::CommandLine::ForCurrentProcess();
    const bool is_running_test =
        current_command_line->HasSwitch(::switches::kTestName) ||
        current_command_line->HasSwitch(::switches::kTestType);
    if (is_running_test) {
      DLOG(WARNING) << "Ignoring chrome restart for test.";
      return;
    }

    // Relaunch chrome without session manager on dev box.
    ReLaunch(command_line);
    return;
  }

  // ChromeRestartRequest deletes itself after request sent to session manager.
  (new ChromeRestartRequest(command_line.argv()))->Start();
}

}  // namespace chromeos
