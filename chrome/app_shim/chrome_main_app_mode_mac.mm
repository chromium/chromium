// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On Mac, one can't make shortcuts with command-line arguments. Instead, we
// produce small app bundles which locate the Chromium framework and load it,
// passing the appropriate data. This is the entry point into the framework for
// those app bundles.

#import <Cocoa/Cocoa.h>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#import "base/mac/launch_services_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread.h"
#include "chrome/app_shim/app_shim_controller.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Timeout in seconds to wait for a reply for the initial Apple Event. Note that
// kAEDefaultTimeout on Mac is "about one minute" according to Apple's
// documentation, but is no longer supported for asynchronous Apple Events.
const int kPingChromeTimeoutSeconds = 60;

}  // namespace

// A ReplyEventHandler is a helper class to send an Apple Event to a process
// and call a callback when the reply returns.
//
// This is used to 'ping' the main Chrome process -- once Chrome has sent back
// an Apple Event reply, it's guaranteed that it has opened the IPC channel
// that the app shim will connect to.
@interface ReplyEventHandler : NSObject {
  base::Callback<void(bool)> onReply_;
  AEDesc replyEvent_;
}
// Sends an Apple Event to the process identified by the bundle identifier,
// and calls |replyFn| when the reply is received. Internally this
// creates a ReplyEventHandler, which will delete itself once the reply event
// has been received.
+ (void)pingProcessAndCall:(base::Callback<void(bool)>)replyFn;
@end

@interface ReplyEventHandler (PrivateMethods)
// Initialise the reply event handler. Doesn't register any handlers until
// |-pingProcess:| is called. |replyFn| is the function to be called when the
// Apple Event reply arrives.
- (id)initWithCallback:(base::Callback<void(bool)>)replyFn;

// Sends an Apple Event ping to the process identified by the bundle
// identifier and registers to listen for a reply.
- (void)pingProcess;

// Called when a response is received from the target process for the ping sent
// by |-pingProcess:|.
- (void)message:(NSAppleEventDescriptor*)event
      withReply:(NSAppleEventDescriptor*)reply;

// Calls |onReply_|, passing it |success| to specify whether the ping was
// successful.
- (void)closeWithSuccess:(bool)success;
@end

@implementation ReplyEventHandler
+ (void)pingProcessAndCall:(base::Callback<void(bool)>)replyFn {
  // The object will release itself when the reply arrives, or possibly earlier
  // if an unrecoverable error occurs.
  ReplyEventHandler* handler =
      [[ReplyEventHandler alloc] initWithCallback:replyFn];
  [handler pingProcess];
}
@end

@implementation ReplyEventHandler (PrivateMethods)
- (id)initWithCallback:(base::Callback<void(bool)>)replyFn {
  if ((self = [super init])) {
    onReply_ = replyFn;
  }
  return self;
}

- (void)pingProcess {
  // Register the reply listener.
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em setEventHandler:self
          andSelector:@selector(message:withReply:)
        forEventClass:kCoreEventClass
           andEventID:kAEAnswer];

  // Craft the Apple Event to send.
  NSString* chromeBundleId = [base::mac::OuterBundle() bundleIdentifier];
  NSAppleEventDescriptor* target = [NSAppleEventDescriptor
      descriptorWithDescriptorType:typeApplicationBundleID
                              data:[chromeBundleId
                                       dataUsingEncoding:NSUTF8StringEncoding]];

  NSAppleEventDescriptor* initialEvent = [NSAppleEventDescriptor
      appleEventWithEventClass:app_mode::kAEChromeAppClass
                       eventID:app_mode::kAEChromeAppPing
              targetDescriptor:target
                      returnID:kAutoGenerateReturnID
                 transactionID:kAnyTransactionID];

  // Note that AESendMessage effectively ignores kAEDefaultTimeout, because this
  // call does not pass kAEWantReceipt (which is deprecated and unsupported on
  // Mac). Instead, rely on OnPingChromeTimeout().
  OSStatus status = AESendMessage([initialEvent aeDesc], &replyEvent_,
                                  kAEQueueReply, kAEDefaultTimeout);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "AESendMessage";
    [self closeWithSuccess:false];
  }
}

- (void)message:(NSAppleEventDescriptor*)event
      withReply:(NSAppleEventDescriptor*)reply {
  [self closeWithSuccess:true];
}

- (void)closeWithSuccess:(bool)success {
  onReply_.Run(success);
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em removeEventHandlerForEventClass:kCoreEventClass andEventID:kAEAnswer];
  [self release];
}
@end

extern "C" {

// |ChromeAppModeStart()| is the point of entry into the framework from the app
// mode loader. There are cases where the Chromium framework may have changed in
// a way that is incompatible with an older shim (e.g. change to libc++ library
// linking). The function name is versioned to provide a way to force shim
// upgrades if they are launched before an updated version of Chromium can
// upgrade them; the old shim will not be able to dyload the new
// ChromeAppModeStart, so it will fall back to the upgrade path. See
// https://crbug.com/561205.
__attribute__((visibility("default")))
int ChromeAppModeStart_v4(const app_mode::ChromeAppModeInfo* info);

}  // extern "C"

int ChromeAppModeStart_v4(const app_mode::ChromeAppModeInfo* info) {
  base::CommandLine::Init(info->argc, info->argv);

  base::mac::ScopedNSAutoreleasePool scoped_pool;
  base::AtExitManager exit_manager;
  chrome::RegisterPathProvider();

  if (info->major_version < app_mode::kCurrentChromeAppModeInfoMajorVersion) {
    RAW_LOG(ERROR, "App Mode Loader too old.");
    return 1;
  }
  if (info->major_version > app_mode::kCurrentChromeAppModeInfoMajorVersion) {
    RAW_LOG(ERROR, "Browser Framework too old to load App Shortcut.");
    return 1;
  }

  // Set bundle paths. This loads the bundles.
  base::mac::SetOverrideOuterBundlePath(info->chrome_outer_bundle_path);
  base::mac::SetOverrideFrameworkBundlePath(
      info->chrome_versioned_path.Append(chrome::kFrameworkName));

  // Calculate the preferred locale used by Chrome.
  // We can't use l10n_util::OverrideLocaleWithCocoaLocale() because it calls
  // [base::mac::OuterBundle() preferredLocalizations] which gets localizations
  // from the bundle of the running app (i.e. it is equivalent to
  // [[NSBundle mainBundle] preferredLocalizations]) instead of the target
  // bundle.
  NSArray* preferred_languages = [NSLocale preferredLanguages];
  NSArray* supported_languages = [base::mac::OuterBundle() localizations];
  std::string preferred_localization;
  for (NSString* language in preferred_languages) {
    if ([supported_languages containsObject:language]) {
      preferred_localization = base::SysNSStringToUTF8(language);
      break;
    }
  }
  std::string locale = l10n_util::NormalizeLocale(
      l10n_util::GetApplicationLocale(preferred_localization));

  // Load localized strings.
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      locale, NULL, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

  // Launch the IO thread.
  base::Thread::Options io_thread_options;
  io_thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  base::Thread *io_thread = new base::Thread("CrAppShimIO");
  io_thread->StartWithOptions(io_thread_options);

  mojo::core::Init();
  mojo::core::ScopedIPCSupport ipc_support(
      io_thread->task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  // Find already running instances of Chrome.
  pid_t pid = -1;
  std::string chrome_process_id =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          app_mode::kLaunchedByChromeProcessId);
  if (!chrome_process_id.empty()) {
    if (!base::StringToInt(chrome_process_id, &pid))
      LOG(FATAL) << "Invalid PID: " << chrome_process_id;
  } else {
    NSString* chrome_bundle_id = [base::mac::OuterBundle() bundleIdentifier];
    NSArray* existing_chrome = [NSRunningApplication
        runningApplicationsWithBundleIdentifier:chrome_bundle_id];
    if ([existing_chrome count] > 0)
      pid = [[existing_chrome objectAtIndex:0] processIdentifier];
  }

  base::MessageLoopForUI main_message_loop;
  ui::WindowResizeHelperMac::Get()->Init(main_message_loop.task_runner());
  base::PlatformThread::SetName("CrAppShimMain");
  AppShimController controller(info);

  // In tests, launching Chrome does nothing, and we won't get a ping response,
  // so just assume the socket exists.
  if (pid == -1 &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          app_mode::kLaunchedForTest)) {
    // Launch Chrome if it isn't already running.
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(switches::kSilentLaunch);

    // If the shim is the app launcher, pass --show-app-list when starting a new
    // Chrome process to inform startup codepaths and load the correct profile.
    if (info->app_mode_id == app_mode::kAppListModeId) {
      command_line.AppendSwitch(switches::kShowAppList);
    } else {
      command_line.AppendSwitchPath(switches::kProfileDirectory,
                                    info->profile_dir);
    }

    base::Process app = base::mac::OpenApplicationWithPath(
        base::mac::OuterBundlePath(), command_line, NSWorkspaceLaunchDefault);
    if (!app.IsValid())
      return 1;

    base::Callback<void(bool)> on_ping_chrome_reply = base::Bind(
        &AppShimController::OnPingChromeReply, base::Unretained(&controller));

    // This code abuses the fact that Apple Events sent before the process is
    // fully initialized don't receive a reply until its run loop starts. Once
    // the reply is received, Chrome will have opened its IPC port, guaranteed.
    [ReplyEventHandler pingProcessAndCall:on_ping_chrome_reply];

    main_message_loop.task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&AppShimController::OnPingChromeTimeout,
                              base::Unretained(&controller)),
        base::TimeDelta::FromSeconds(kPingChromeTimeoutSeconds));
  } else {
    // Chrome already running. Proceed to init. This could still fail if Chrome
    // is still starting up or shutting down, but the process will exit quickly,
    // which is preferable to waiting for the Apple Event to timeout after one
    // minute.
    main_message_loop.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&AppShimController::InitBootstrapPipe,
                                  base::Unretained(&controller)));
  }

  base::RunLoop().Run();
  return 0;
}
