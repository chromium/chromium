// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/test_support_ios.h"

#import <UIKit/UIKit.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_mac.h"
#import "base/test/ios/google_test_runner_delegate.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "testing/coverage_util_ios.h"

// Springboard will kill any iOS app that fails to check in after launch within
// a given time. Starting a UIApplication before invoking TestSuite::Run
// prevents this from happening.

// InitIOSRunHook saves the TestSuite and argc/argv, then invoking
// RunTestsFromIOSApp calls UIApplicationMain(), providing an application
// delegate class: ChromeUnitTestDelegate. The delegate implements
// application:didFinishLaunchingWithOptions: to invoke the TestSuite's Run
// method.

// Since the executable isn't likely to be a real iOS UI, the delegate puts up a
// window displaying the app name. If a bunch of apps using MainHook are being
// run in a row, this provides an indication of which one is currently running.

static base::TestSuite* g_test_suite = NULL;
static int g_argc;
static char** g_argv;

namespace {
void PopulateUIWindow(UIWindow* window) {
  [window setBackgroundColor:[UIColor whiteColor]];
  [window makeKeyAndVisible];
  CGRect bounds = [[UIScreen mainScreen] bounds];
  // Add a label with the app name.
  UILabel* label = [[[UILabel alloc] initWithFrame:bounds] autorelease];
  label.text = [[NSProcessInfo processInfo] processName];
  label.textAlignment = NSTextAlignmentCenter;
  [window addSubview:label];

  // An NSInternalInconsistencyException is thrown if the app doesn't have a
  // root view controller. Set an empty one here.
  [window setRootViewController:[[[UIViewController alloc] init] autorelease]];
}

bool IsSceneStartupEnabled() {
  return [[NSBundle mainBundle].infoDictionary
      objectForKey:@"UIApplicationSceneManifest"];
}
}

@interface UIApplication (Testing)
- (void)_terminateWithStatus:(int)status;
@end

#if TARGET_IPHONE_SIMULATOR
// Xcode 6 introduced behavior in the iOS Simulator where the software
// keyboard does not appear if a hardware keyboard is connected. The following
// declaration allows this behavior to be overriden when the app starts up.
@interface UIKeyboardImpl
+ (instancetype)sharedInstance;
- (void)setAutomaticMinimizationEnabled:(BOOL)enabled;
- (void)setSoftwareKeyboardShownByTouch:(BOOL)enabled;
@end
#endif  // TARGET_IPHONE_SIMULATOR

// No-op scene delegate for unit tests. Note that this is created along with
// the application delegate, so they need to be separate objects (the same
// object can't be both the app and scene delegate, since new scene delegates
// are created for each scene).
@interface ChromeUnitTestSceneDelegate : NSObject <UIWindowSceneDelegate> {
  base::scoped_nsobject<UIWindow> _window;
}

@end

@interface ChromeUnitTestDelegate : NSObject <GoogleTestRunnerDelegate> {
  base::scoped_nsobject<UIWindow> _window;
}
- (void)runTests;
@end

@implementation ChromeUnitTestSceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions
    API_AVAILABLE(ios(13)) {
  // Unittests do not support multiple scenes.
  DCHECK(![[UIApplication sharedApplication] supportsMultipleScenes]);
  // Yes, this is leaked, it's just to make what's running visible.
  _window.reset([[UIWindow alloc]
      initWithWindowScene:static_cast<UIWindowScene*>(scene)]);
  PopulateUIWindow(_window);
}

- (void)sceneDidDisconnect:(UIScene*)scene API_AVAILABLE(ios(13)) {
  _window.reset();
}

@end

@implementation ChromeUnitTestDelegate

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {

#if TARGET_IPHONE_SIMULATOR
  // Xcode 6 introduced behavior in the iOS Simulator where the software
  // keyboard does not appear if a hardware keyboard is connected. The following
  // calls override this behavior by ensuring that the software keyboard is
  // always shown.
  [[UIKeyboardImpl sharedInstance] setAutomaticMinimizationEnabled:NO];
  if (@available(iOS 15, *)) {
  } else {
    [[UIKeyboardImpl sharedInstance] setSoftwareKeyboardShownByTouch:YES];
  }
#endif  // TARGET_IPHONE_SIMULATOR

  if (!IsSceneStartupEnabled()) {
    CGRect bounds = [[UIScreen mainScreen] bounds];

    // Yes, this is leaked, it's just to make what's running visible.
    _window.reset([[UIWindow alloc] initWithFrame:bounds]);
    PopulateUIWindow(_window);
  }

  if ([self shouldRedirectOutputToFile])
    [self redirectOutput];

  // Queue up the test run.
  if (!base::ShouldRunIOSUnittestsWithXCTest()) {
    // When running in XCTest mode, XCTest will invoke |runGoogleTest| directly.
    // Otherwise, schedule a call to |runTests|.
    [self performSelector:@selector(runTests) withObject:nil afterDelay:0.1];
  }

  return YES;
}

// Returns true if the gtest output should be redirected to a file, then sent
// to NSLog when complete. This redirection is used because gtest only writes
// output to stdout, but results must be written to NSLog in order to show up in
// the device log that is retrieved from the device by the host.
- (BOOL)shouldRedirectOutputToFile {
#if !TARGET_IPHONE_SIMULATOR
  // Tests in XCTest mode don't need to redirect output to a file because the
  // test result parser analyzes console output.
  return !base::ShouldRunIOSUnittestsWithXCTest() &&
         !base::debug::BeingDebugged();
#else
  return NO;
#endif  // TARGET_IPHONE_SIMULATOR
}

// Returns the path to the directory to store gtest output files.
- (NSString*)outputPath {
  NSArray* searchPath =
      NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                          NSUserDomainMask,
                                          YES);
  CHECK([searchPath count] > 0) << "Failed to get the Documents folder";
  return [searchPath objectAtIndex:0];
}

// Returns the path to file that stdout is redirected to.
- (NSString*)stdoutPath {
  return [[self outputPath] stringByAppendingPathComponent:@"stdout.log"];
}

// Returns the path to file that stderr is redirected to.
- (NSString*)stderrPath {
  return [[self outputPath] stringByAppendingPathComponent:@"stderr.log"];
}

// Redirects stdout and stderr to files in the Documents folder in the app's
// sandbox.
- (void)redirectOutput {
  freopen([[self stdoutPath] UTF8String], "w+", stdout);
  freopen([[self stderrPath] UTF8String], "w+", stderr);
}

// Reads the redirected gtest output from a file and writes it to NSLog.
- (void)writeOutputToNSLog {
  // Close the redirected stdout and stderr files so that the content written to
  // NSLog doesn't end up in these files.
  fclose(stdout);
  fclose(stderr);
  for (NSString* path in @[ [self stdoutPath], [self stderrPath]]) {
    NSString* content = [NSString stringWithContentsOfFile:path
                                                  encoding:NSUTF8StringEncoding
                                                     error:NULL];
    NSArray* lines = [content componentsSeparatedByCharactersInSet:
        [NSCharacterSet newlineCharacterSet]];

    NSLog(@"Writing contents of %@ to NSLog", path);
    for (NSString* line in lines) {
      NSLog(@"%@", line);
    }
  }
}

- (BOOL)supportsRunningGoogleTests {
  return base::ShouldRunIOSUnittestsWithXCTest();
}

- (int)runGoogleTests {
  coverage_util::ConfigureCoverageReportPath();

  int exitStatus = g_test_suite->Run();

  if ([self shouldRedirectOutputToFile])
    [self writeOutputToNSLog];

  return exitStatus;
}

- (void)runTests {
  DCHECK(!base::ShouldRunIOSUnittestsWithXCTest());

  int exitStatus = [self runGoogleTests];

  // If a test app is too fast, it will exit before Instruments has has a
  // a chance to initialize and no test results will be seen.
  [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.0]];
  _window.reset();

  // Use the hidden selector to try and cleanly take down the app (otherwise
  // things can think the app crashed even on a zero exit status).
  UIApplication* application = [UIApplication sharedApplication];
  [application _terminateWithStatus:exitStatus];

  exit(exitStatus);
}

@end

namespace {

std::unique_ptr<base::MessagePump> CreateMessagePumpForUIForTests() {
  // A basic MessagePump will do quite nicely in tests.
  return std::unique_ptr<base::MessagePump>(new base::MessagePumpCFRunLoop());
}

}  // namespace

namespace base {

void InitIOSTestMessageLoop() {
  MessagePump::OverrideMessagePumpForUIFactory(&CreateMessagePumpForUIForTests);
}

void InitIOSRunHook(TestSuite* suite, int argc, char* argv[]) {
  g_test_suite = suite;
  g_argc = argc;
  g_argv = argv;
}

void RunTestsFromIOSApp() {
  // When TestSuite::Run is invoked it calls RunTestsFromIOSApp(). On the first
  // invocation, this method fires up an iOS app via UIApplicationMain. Since
  // UIApplicationMain does not return until the app exits, control does not
  // return to the initial TestSuite::Run invocation, so the app invokes
  // TestSuite::Run a second time and since |ran_hook| is true at this point,
  // this method is a no-op and control returns to TestSuite:Run so that test
  // are executed. Once the app exits, RunTestsFromIOSApp calls exit() so that
  // control is not returned to the initial invocation of TestSuite::Run.
  static bool ran_hook = false;
  if (!ran_hook) {
    ran_hook = true;
    @autoreleasepool {
      int exit_status =
          UIApplicationMain(g_argc, g_argv, nil, @"ChromeUnitTestDelegate");
      exit(exit_status);
    }
  }
}

bool ShouldRunIOSUnittestsWithXCTest() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableRunIOSUnittestsWithXCTest);
}

}  // namespace base
