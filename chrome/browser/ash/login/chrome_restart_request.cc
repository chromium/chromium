// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/chrome_restart_request.h"

#include <sys/socket.h>

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/standalone_browser/channel_util.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/user_manager/user_names.h"
#include "components/viz/common/switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/switches.h"
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

namespace ash {

namespace {

using ::content::BrowserThread;

// Increase logging level for Guest mode to avoid INFO messages in logs.
const char kGuestModeLoggingLevel[] = "1";

bool IsRunningTest() {
  const base::CommandLine* current_command_line =
      base::CommandLine::ForCurrentProcess();
  return current_command_line->HasSwitch(::switches::kTestName) ||
         current_command_line->HasSwitch(::switches::kTestType);
}

// Derives the new command line from `base_command_line` by doing the following:
// - Forward a given switches list to new command;
// - Set start url if given;
// - Append/override switches using `new_switches`;
void DeriveCommandLine(const GURL& start_url,
                       const base::CommandLine& base_command_line,
                       const base::Value::Dict& new_switches,
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
      ::switches::kDisableMojoBroker,
      ::switches::kDisableTouchDragDrop,
      ::switches::kDisableVideoCaptureUseGpuMemoryBuffer,
      ::switches::kDisableYUVImageDecoding,
      ::switches::kEnableBlinkFeatures,
      ::switches::kEnableGpuMemoryBufferVideoFrames,
      ::switches::kEnableGpuRasterization,
      ::switches::kEnableLogging,
      ::switches::kEnableMicrophoneMuteSwitchDeviceSwitch,
      ::switches::kEnableNativeGpuMemoryBuffers,
      ::switches::kEnableTouchDragDrop,
      ::switches::kEnableUnifiedDesktop,
      ::switches::kEnableViewport,
      ::switches::kEnableHardwareOverlays,
      ::switches::kEdgeTouchFiltering,
      ::switches::kHostWindowBounds,
      ::switches::kForceDeviceScaleFactor,
      ::switches::kForceGpuMemAvailableMb,
      ::switches::kGpuStartupDialog,
      ::switches::kGpuSandboxStartEarly,
      ::switches::kPpapiInProcess,
      ::switches::kRemoteDebuggingPort,
      ::switches::kRendererStartupDialog,
      ::switches::kSchedulerBoostUrgent,
      ::switches::kSchedulerConfigurationDefault,
      ::switches::kTouchDevices,
      ::switches::kTouchEventFeatureDetection,
      ::switches::kTopChromeTouchUi,
      ::switches::kTraceToConsole,
      ::switches::kUIDisablePartialSwap,
#if BUILDFLAG(USE_CRAS)
      ::switches::kUseCras,
#endif
      ::switches::kUseGL,
      ::switches::kUserDataDir,
      ::switches::kV,
      ::switches::kVModule,
      ::switches::kVideoCaptureUseGpuMemoryBuffer,
      ::switches::kWebAuthRemoteDesktopSupport,
      ::switches::kEnableWebGLDeveloperExtensions,
      ::switches::kEnableWebGLDraftExtensions,
      ::switches::kDisableWebGLImageChromium,
      ::switches::kEnableWebGLImageChromium,
      ::switches::kEnableUnsafeWebGPU,
      ::switches::kEnableWebGPUDeveloperFeatures,
      ::switches::kOzonePlatform,
      switches::kAshClearFastInkBuffer,
      switches::kAshEnablePaletteOnAllDisplays,
      switches::kAshEnableTabletMode,
      switches::kAshEnableWaylandServer,
      switches::kAshForceEnableStylusTools,
      switches::kAshTouchHud,
      switches::kAuraLegacyPowerButton,
      switches::kEnableDimShelf,
      switches::kSupportsClamshellAutoRotation,
      switches::kShowTaps,
      blink::switches::kBlinkSettings,
      blink::switches::kDarkModeSettings,
      blink::switches::kDisableLowResTiling,
      blink::switches::kDisablePartialRaster,
      blink::switches::kDisablePreferCompositingToLCDText,
      blink::switches::kDisableRGBA4444Textures,
      blink::switches::kDisableZeroCopy,
      blink::switches::kEnableLowResTiling,
      blink::switches::kEnablePreferCompositingToLCDText,
      blink::switches::kEnableRGBA4444Textures,
      blink::switches::kEnableRasterSideDarkModeForImages,
      blink::switches::kEnableZeroCopy,
      blink::switches::kGpuRasterizationMSAASampleCount,
      switches::kAshPowerButtonPosition,
      switches::kAshSideVolumeButtonPosition,
      switches::kDefaultWallpaperLarge,
      switches::kDefaultWallpaperSmall,
      switches::kGuestWallpaperLarge,
      switches::kGuestWallpaperSmall,
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
      cc::switches::kNumRasterThreads,
      cc::switches::kShowCompositedLayerBorders,
      cc::switches::kShowFPSCounter,
      cc::switches::kShowLayerAnimationBounds,
      cc::switches::kShowPropertyChangedRects,
      cc::switches::kShowScreenSpaceRects,
      cc::switches::kShowSurfaceDamageRects,
      cc::switches::kSlowDownRasterScaleFactor,
      cc::switches::kUIShowFPSCounter,
      extensions::switches::kLoadGuestModeTestExtension,
      switches::kArcAvailability,
      switches::kArcAvailable,
      switches::kArcScale,
      chromeos::switches::kDbusStub,
      switches::kDisableArcOptInVerification,
      switches::kDisableLoginAnimations,
      switches::kEnableArc,
      switches::kEnterpriseDisableArc,
      switches::kEnterpriseEnableForcedReEnrollment,
      switches::kForceTabletPowerButton,
      switches::kFormFactor,
      switches::kHasChromeOSKeyboard,
      switches::kLacrosChromeAdditionalArgs,
      switches::kLacrosChromeAdditionalEnv,
      switches::kLacrosChromePath,
      ash::standalone_browser::kLacrosStabilitySwitch,
      switches::kLoginProfile,
      switches::kNaturalScrollDefault,
      switches::kOobeForceTabletFirstRun,
      switches::kRlzPingDelay,
      chromeos::switches::kSystemInDevMode,
      switches::kTouchscreenUsableWhileScreenOff,
      policy::switches::kDeviceManagementUrl,
      wm::switches::kWindowAnimationsDisabled,
  };
  command_line->CopySwitchesFrom(base_command_line, kForwardSwitches);

  if (start_url.is_valid())
    command_line->AppendArg(start_url.spec());

  for (auto new_switch : new_switches) {
    command_line->AppendSwitchASCII(new_switch.first,
                                    new_switch.second.GetString());
  }
}

// Adds allowlisted features to `out_command_line` if they are overridden in the
// current session.
void DeriveFeatures(base::CommandLine* out_command_line) {
  auto kForwardFeatures = {
      &features::kAutoNightLight,
      &ash::features::kSeamlessRefreshRateSwitching,
      &ash::standalone_browser::features::kLacrosOnly,
      &::features::kPluginVm,
      &display::features::kOledScaleFactorEnabled,
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
      &media::kPlatformHEVCDecoderSupport,
#endif
  };
  std::vector<std::string> enabled_features;
  std::vector<std::string> disabled_features;
  for (const auto* feature : kForwardFeatures) {
    if (auto state = base::FeatureList::GetStateIfOverridden(*feature)) {
      if (*state) {
        enabled_features.push_back(feature->name);
      } else {
        disabled_features.push_back(feature->name);
      }
    }
  }

  if (!enabled_features.empty()) {
    out_command_line->AppendSwitchASCII(
        "enable-features", base::JoinString(enabled_features, ","));
  }
  if (!disabled_features.empty()) {
    out_command_line->AppendSwitchASCII(
        "disable-features", base::JoinString(disabled_features, ","));
  }
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
class ChromeRestartRequest {
 public:
  explicit ChromeRestartRequest(const std::vector<std::string>& argv,
                                RestartChromeReason reson);

  ChromeRestartRequest(const ChromeRestartRequest&) = delete;
  ChromeRestartRequest& operator=(const ChromeRestartRequest&) = delete;

  ~ChromeRestartRequest();

  // Starts the request.
  void Start();

 private:
  // Fires job restart request to session manager.
  void RestartJob();

  // Called when RestartJob D-Bus method call is complete.
  void OnRestartJob(base::ScopedFD local_auth_fd, bool result);

  const std::vector<std::string> argv_;
  const RestartChromeReason reason_;

  base::OneShotTimer timer_;

  base::WeakPtrFactory<ChromeRestartRequest> weak_ptr_factory_{this};
};

ChromeRestartRequest::ChromeRestartRequest(const std::vector<std::string>& argv,
                                           RestartChromeReason reason)
    : argv_(argv), reason_(reason) {}

ChromeRestartRequest::~ChromeRestartRequest() {}

void ChromeRestartRequest::Start() {
  VLOG(1) << "Requesting a restart with command line: "
          << base::JoinString(argv_, " ");

  // Session Manager may kill the chrome anytime after this point.
  // Write exit_cleanly and other stuff to the disk here.
  g_browser_process->EndSession();

  // XXX: normally this call must not be needed, however RestartJob
  // just kills us so settings may be lost. See http://crosbug.com/13102
  g_browser_process->FlushLocalStateAndReply(base::BindOnce(
      &ChromeRestartRequest::RestartJob, weak_ptr_factory_.GetWeakPtr()));
  timer_.Start(FROM_HERE, base::Seconds(3), this,
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
      static_cast<SessionManagerClient::RestartJobReason>(reason_),
      base::BindOnce(&ChromeRestartRequest::OnRestartJob,
                     weak_ptr_factory_.GetWeakPtr(), std::move(local_auth_fd)));
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
                                const base::CommandLine& base_command_line,
                                base::CommandLine* command_line) {
  base::Value::Dict otr_switches;
  otr_switches.Set(switches::kGuestSession, std::string());
  otr_switches.Set(::switches::kIncognito, std::string());
  otr_switches.Set(::switches::kLoggingLevel, kGuestModeLoggingLevel);
  otr_switches.Set(
      switches::kLoginUser,
      cryptohome::Identification(user_manager::GuestAccountId()).id());
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    otr_switches.Set(switches::kLoginProfile,
                     BrowserContextHelper::kLegacyBrowserContextDirName);
  }

  // Override the home page.
  otr_switches.Set(::switches::kHomePage,
                   GURL(chrome::kChromeUINewTabURL).spec());

  DeriveCommandLine(start_url, base_command_line, otr_switches, command_line);
  DeriveFeatures(command_line);
}

void RestartChrome(const base::CommandLine& command_line,
                   RestartChromeReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BootTimesRecorder::Get()->set_restart_requested();

  static bool restart_requested = false;
  if (restart_requested) {
    NOTREACHED_IN_MIGRATION() << "Request chrome restart for more than once.";
  }
  restart_requested = true;

  if (!SessionManagerClient::Get()->SupportsBrowserRestart()) {
    // Do nothing when running as test on bots or a dev box.
    if (IsRunningTest()) {
      DLOG(WARNING) << "Ignoring chrome restart for test.";
      return;
    }

    // Relaunch chrome without session manager on dev box.
    ReLaunch(command_line);
    return;
  }

  // ChromeRestartRequest deletes itself after request sent to session manager.
  (new ChromeRestartRequest(command_line.argv(), reason))->Start();
}

}  // namespace ash
