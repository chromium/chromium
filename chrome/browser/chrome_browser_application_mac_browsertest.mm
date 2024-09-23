// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/chrome_browser_application_mac.h"

#import "base/apple/foundation_util.h"
#import "base/apple/scoped_objc_class_swizzler.h"
#import "base/mac/mac_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

namespace {
BOOL g_voice_over_enabled = NO;
}

@interface NSWorkspace (Extras)
- (BOOL)voiceOverEnabled;
- (void)setVoiceOverEnabled:(BOOL)flag;
@end

@implementation NSWorkspace (Extras)

- (BOOL)voiceOverEnabled {
  return g_voice_over_enabled;
}

// It seems NSWorkspace notifies of changes to voiceOverEnabled via KVO,
// but doesn't implement this method. We add it so we can test our KVO
// monitoring code.
- (void)setVoiceOverEnabled:(BOOL)flag {
  g_voice_over_enabled = flag;
}

@end

@interface NSApplication (ChromeBrowserApplicationMacBrowserTestSwizzle)
@end

@implementation NSApplication (ChromeBrowserApplicationMacBrowserTestSwizzle)

- (void)testObserveValueForKeyPath:(NSString*)keyPath
                          ofObject:(id)object
                            change:
                                (NSDictionary<NSKeyValueChangeKey, id>*)change
                           context:(void*)context {
  if (context) {
    *static_cast<bool*>(context) = true;
  }
}

@end

class ChromeBrowserAppMacBrowserTest : public InProcessBrowserTest {
 public:
  ChromeBrowserAppMacBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSonomaAccessibilityActivationRefinements);

    br_cr_app_ = base::apple::ObjCCast<BrowserCrApplication>(NSApp);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SetVoiceOverEnabled(VoiceOverEnabledAtStartUp());
  }

  // Whether or not we simulate VoiceOver active before the test runs.
  virtual BOOL VoiceOverEnabledAtStartUp() { return NO; }

  BOOL VoiceOverEnabled() { return [br_cr_app_ voiceOverStateForTesting]; }

  // Simulates the user activating or deactivating VoiceOver.
  void SetVoiceOverEnabled(BOOL enabled) {
    NSString* kVoiceOverKVOKeyPath = @"voiceOverEnabled";
    [[NSWorkspace sharedWorkspace] setValue:[NSNumber numberWithBool:enabled]
                                     forKey:kVoiceOverKVOKeyPath];
  }

  void WaitThreeSeconds() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&base::RunLoop::Quit, base::Unretained(&run_loop)),
        base::Seconds(3));
    run_loop.Run();
  }

  bool BrowserIsInAccessibilityMode(ui::AXMode mode) {
    content::BrowserAccessibilityState* accessibility_state =
        content::BrowserAccessibilityState::GetInstance();
    return accessibility_state->GetAccessibilityMode() == mode;
  }

  bool BrowserIsInCompleteAccessibilityMode() {
    return BrowserIsInAccessibilityMode(ui::kAXModeComplete);
  }

  bool BrowserIsInBasicAccessibilityMode() {
    return BrowserIsInAccessibilityMode(ui::kAXModeBasic);
  }

  bool BrowserIsInNativeAPIAccessibilityMode() {
    return BrowserIsInAccessibilityMode(ui::AXMode::kNativeAPIs);
  }

  bool BrowserAccessibilityDisabled() {
    return BrowserIsInAccessibilityMode(ui::AXMode());
  }

  void RequestAppAccessibilityRole() { [br_cr_app_ accessibilityRole]; }

  void EnableEnhancedUserInterface(BOOL enable) {
    // We need to call -accessibilitySetValue:forAttribute: on br_cr_app_, but
    // the compiler complains it's deprecated API. It's right, but it's the API
    // BrowserCrApplication is currently using. Silence the error.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [br_cr_app_ accessibilitySetValue:[NSNumber numberWithBool:enable]
                         forAttribute:@"AXEnhancedUserInterface"];
#pragma clang diagnostic pop
  }

 private:
  BrowserCrApplication* br_cr_app_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensures that overrides to the application's
// observeValueForKeyPath:ofObject:change:context: method call super on
// unrecognized key paths.
IN_PROC_BROWSER_TEST_F(ChromeBrowserAppMacBrowserTest,
                       KVOObservationCallsSuper) {
  base::apple::ScopedObjCClassSwizzler swizzler(
      [NSApplication class],
      @selector(observeValueForKeyPath:ofObject:change:context:),
      @selector(testObserveValueForKeyPath:ofObject:change:context:));

  bool super_was_called = false;

  [NSApp observeValueForKeyPath:@"testKeyPath"
                       ofObject:nil
                         change:nil
                        context:&super_was_called];

  EXPECT_TRUE(super_was_called);
}

// Tests how BrowserCrApplication responds to VoiceOver activations.
IN_PROC_BROWSER_TEST_F(ChromeBrowserAppMacBrowserTest,
                       RespondToVoiceOverStateChanges) {
  // We could perform this check in SetUp(), but in theory, this browser
  // test file will contain more that just these Sonoma tests.
  if (base::mac::MacOSVersion() < 14'00'00) {
    GTEST_SKIP();
  }

  EXPECT_FALSE(VoiceOverEnabled());
  EXPECT_TRUE(BrowserAccessibilityDisabled());

  SetVoiceOverEnabled(YES);
  EXPECT_TRUE(VoiceOverEnabled());
  EXPECT_TRUE(BrowserIsInCompleteAccessibilityMode());

  SetVoiceOverEnabled(NO);
  EXPECT_FALSE(VoiceOverEnabled());

  // Turning VoiceOver off disables accessibility support, but not immediately.
  // Chrome waits a couple seconds in case there's a fast-follow call to enable
  // it. Wait a bit for the change to take before proceeding.
  WaitThreeSeconds();

  EXPECT_TRUE(BrowserAccessibilityDisabled());
}

// Tests how BrowserCrApplication responds to AXEnhancedUserInterface requests
// from Assistive Technology (AT).
IN_PROC_BROWSER_TEST_F(ChromeBrowserAppMacBrowserTest,
                       RespondToAXEnhancedUserInterfaceRequests) {
  if (base::mac::MacOSVersion() < 14'00'00) {
    GTEST_SKIP();
  }

  EXPECT_TRUE(BrowserAccessibilityDisabled());

  // Requesting AX enhanced UI should have no immediate effect.
  EnableEnhancedUserInterface(YES);
  EXPECT_TRUE(BrowserAccessibilityDisabled());

  // If we suddenly turn off support and wait a bit, there should be no change
  // in accessibility support. We're ensuring that sudden on/off changes are
  // ignored.
  EnableEnhancedUserInterface(NO);
  WaitThreeSeconds();
  EXPECT_TRUE(BrowserAccessibilityDisabled());

  // If we turn it on and wait, the code should assume it's not a spurious
  // request.
  EnableEnhancedUserInterface(YES);
  WaitThreeSeconds();
  EXPECT_TRUE(BrowserIsInCompleteAccessibilityMode());

  // Turn it off (and wait a bit).
  EnableEnhancedUserInterface(NO);
  WaitThreeSeconds();
  EXPECT_TRUE(BrowserAccessibilityDisabled());
}

// Tests how BrowserCrApplication responds to mismatched
// AXEnhancedUserInterface requests.
IN_PROC_BROWSER_TEST_F(ChromeBrowserAppMacBrowserTest,
                       HandleMismatchedAXEnhancedUserInterfaceRequests) {
  if (base::mac::MacOSVersion() < 14'00'00) {
    GTEST_SKIP();
  }

  EXPECT_TRUE(BrowserAccessibilityDisabled());

  // The code uses a counter to track requests. Ensure that it can't go
  // negative.
  EnableEnhancedUserInterface(YES);
  EnableEnhancedUserInterface(NO);
  EnableEnhancedUserInterface(NO);
  EnableEnhancedUserInterface(NO);

  WaitThreeSeconds();

  EnableEnhancedUserInterface(YES);
  WaitThreeSeconds();
  EXPECT_TRUE(BrowserIsInCompleteAccessibilityMode());
}

// Tests that BrowserCrApplication ignores requests from ATs to disable AX
// support if VoiceOver is active.
IN_PROC_BROWSER_TEST_F(ChromeBrowserAppMacBrowserTest,
                       IgnoreAXEnhancedUserInterfaceDisableRequests) {
  if (base::mac::MacOSVersion() < 14'00'00) {
    GTEST_SKIP();
  }

  EXPECT_TRUE(BrowserAccessibilityDisabled());

  // Simulate an AT requesting AX enhanced UI.
  EnableEnhancedUserInterface(YES);

  WaitThreeSeconds();

  // The user activates VoiceOver.
  SetVoiceOverEnabled(YES);
  EXPECT_TRUE(BrowserIsInCompleteAccessibilityMode());

  // When the AT is done, make sure it can't disable AX support (VoiceOver is
  // using it).
  EnableEnhancedUserInterface(NO);
  WaitThreeSeconds();
  EXPECT_FALSE(BrowserAccessibilityDisabled());
}

// Tests that accessibility role requests to the application enable native
// accessibility support.
IN_PROC_BROWSER_TEST_F(ChromeBrowserAppMacBrowserTest,
                       RespondToAccessibilityRoleRequests) {
  if (base::mac::MacOSVersion() < 14'00'00) {
    GTEST_SKIP();
  }

  EXPECT_TRUE(BrowserAccessibilityDisabled());

  RequestAppAccessibilityRole();
  EXPECT_TRUE(BrowserIsInNativeAPIAccessibilityMode());

  // The user activates VoiceOver.
  SetVoiceOverEnabled(YES);

  // Requests for AccessibilityRole when VoiceOver is active should not
  // downgrade the AX level.
  RequestAppAccessibilityRole();
  EXPECT_TRUE(BrowserIsInCompleteAccessibilityMode());

  SetVoiceOverEnabled(NO);
  WaitThreeSeconds();
  EXPECT_TRUE(BrowserAccessibilityDisabled());

  EnableEnhancedUserInterface(YES);
  WaitThreeSeconds();

  // Requests for AccessibilityRole when the AX mode is complete should not
  // downgrade the AX level.
  RequestAppAccessibilityRole();
  EXPECT_TRUE(BrowserIsInCompleteAccessibilityMode());
}

// A test class where VoiceOver is "enabled" when its tests start.
class ChromeBrowserAppMacBrowserMacVoiceOverEnabledTest
    : public ChromeBrowserAppMacBrowserTest {
 public:
  BOOL VoiceOverEnabledAtStartUp() override { return YES; }
};

IN_PROC_BROWSER_TEST_F(ChromeBrowserAppMacBrowserMacVoiceOverEnabledTest,
                       DetectVoiceOverStateOnStartUp) {
  if (base::mac::MacOSVersion() < 14'00'00) {
    GTEST_SKIP();
  }

  content::BrowserAccessibilityState* accessibility_state =
      content::BrowserAccessibilityState::GetInstance();

  // Enable VoiceOver.
  EXPECT_TRUE(VoiceOverEnabled());
  EXPECT_EQ(accessibility_state->GetAccessibilityMode(), ui::kAXModeComplete);
}
