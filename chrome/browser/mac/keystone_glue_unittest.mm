// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/test/task_environment.h"
#import "chrome/browser/mac/keystone_glue.h"
#import "chrome/browser/mac/keystone_registration.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace ksr = keystone_registration;


@interface FakeKeystoneRegistration : KSRegistration
@end


// This unit test implements FakeKeystoneRegistration as a KSRegistration
// subclass. It won't be linked against KSRegistration, so provide a stub
// KSRegistration class on which to base FakeKeystoneRegistration.
@implementation KSRegistration

+ (instancetype)registrationWithProductID:(NSString*)productID {
  return nil;
}

- (BOOL)registerWithParameters:(NSDictionary*)args {
  NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
  [center postNotificationName:ksr::KSRegistrationDidCompleteNotification
                        object:nil
                      userInfo:@{ksr::KSRegistrationStatusKey : @1}];
  return YES;
}

- (BOOL)promoteWithParameters:(NSDictionary*)args
                authorization:(AuthorizationRef)authorization {
  return NO;
}

- (BOOL)setActiveWithError:(NSError**)error {
  return NO;
}

- (void)checkForUpdateWasUserInitiated:(BOOL)userInitiated {
}

- (void)startUpdate {
}

- (ksr::KSRegistrationTicketType)ticketType {
  return ksr::kKSRegistrationDontKnowWhatKindOfTicket;
}

@end


@implementation FakeKeystoneRegistration

// Send the notifications that a real KeystoneGlue object would send.

- (void)checkForUpdateWasUserInitiated:(BOOL)userInitiated {
  NSString* statusKey = @"Status";
  NSDictionary* dictionary = @{statusKey : @YES};
  [NSNotificationCenter.defaultCenter
      postNotificationName:ksr::KSRegistrationCheckForUpdateNotification
                    object:nil
                  userInfo:dictionary];
}

- (void)startUpdate {
  [NSNotificationCenter.defaultCenter
      postNotificationName:ksr::KSRegistrationStartUpdateNotification
                    object:nil
                  userInfo:@{ksr::KSUpdateCheckSuccessfullyInstalledKey : @1}];
}

@end


@interface FakeKeystoneGlue : KeystoneGlue {
 @public
  BOOL _upToDate;
  NSString* __strong _latestVersion;
  BOOL _successful;
  int _installs;
}

- (void)fakeAboutWindowCallback:(NSNotification*)notification;
@end


@implementation FakeKeystoneGlue

- (instancetype)init {
  if ((self = [super init])) {
    // some lies
    _upToDate = YES;
    _latestVersion = @"foo bar";
    _successful = YES;
    _installs = 1010101010;

    // Set up an observer that takes the notification that the About window
    // listens for.
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(fakeAboutWindowCallback:)
               name:kAutoupdateStatusNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

// For mocking
- (NSDictionary*)infoDictionary {
  return @{
    @"KSUpdateURL" : @"http://foo.bar",
    @"KSProductID" : @"com.google.whatever",
    @"KSVersion" : @"0.0.0.1"
  };
}

- (BOOL)loadKeystoneRegistration {
  // The real loadKeystoneRegistration adds a real registration.
  // Add a fake one.
  [self setKeystoneRegistration:[[FakeKeystoneRegistration alloc] init]];
  return YES;
}

// Confirms certain things are happy
- (BOOL)dictReadCorrectly {
  return ([self.url isEqual:@"http://foo.bar"] &&
          [self.productID isEqual:@"com.google.whatever"] &&
          [self.version isEqual:@"0.0.0.1"]);
}

// Confirms certain things are happy
- (BOOL)hasATimer {
  return self.timer ? YES : NO;
}

- (void)fakeAboutWindowCallback:(NSNotification*)notification {
  NSDictionary* dictionary = notification.userInfo;
  AutoupdateStatus status = static_cast<AutoupdateStatus>(
      [dictionary[kAutoupdateStatusStatus] intValue]);

  if (status == kAutoupdateAvailable) {
    _upToDate = NO;
    _latestVersion = [dictionary[kAutoupdateStatusVersion] copy];
  } else if (status == kAutoupdateInstallFailed) {
    _successful = NO;
    _installs = 0;
  }
}

@end

@interface KeystoneGlue (PrivateMethods)

+ (BOOL)isValidSystemKeystone:(NSDictionary*)systemKeystonePlistContents
            comparedToBundled:(NSDictionary*)bundledKeystonePlistContents;

@end

namespace {

class KeystoneGlueTest : public PlatformTest {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(KeystoneGlueTest, BasicGlobalCreate) {
  // Allow creation of a KeystoneGlue by mocking out a couple calls.
  base::apple::ScopedObjCClassSwizzler infoDictionarySwizzler(
      [KeystoneGlue class], [FakeKeystoneGlue class],
      @selector(infoDictionary));
  base::apple::ScopedObjCClassSwizzler loadRegistrationSwizzler(
      [KeystoneGlue class], [FakeKeystoneGlue class],
      @selector(loadKeystoneRegistration));

  KeystoneGlue *glue = [KeystoneGlue defaultKeystoneGlue];
  ASSERT_TRUE(glue);
}

TEST_F(KeystoneGlueTest, BasicUse) {
  FakeKeystoneGlue* glue = [[FakeKeystoneGlue alloc] init];
  [glue loadParameters];
  ASSERT_TRUE([glue dictReadCorrectly]);

  // Likely returns NO in the unit test, but call it anyway to make
  // sure it doesn't crash.
  [glue loadKeystoneRegistration];

  // Confirm we start up an active timer
  [glue registerWithKeystone];
  ASSERT_TRUE([glue hasATimer]);
  [glue stopTimer];

  // Checking for an update should succeed, yielding kAutoupdateAvailable:
  [glue checkForUpdate];
  ASSERT_EQ([glue recentStatus], kAutoupdateAvailable);

  // And applying the update should also succeed:
  [glue installUpdate];
  ASSERT_EQ([glue recentStatus], kAutoupdateInstalling);
  // The rest of the update install is asynchronous & happens on a worker
  // thread, so don't check it here.
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Nils) {
  ASSERT_TRUE([KeystoneGlue isValidSystemKeystone:nil comparedToBundled:nil]);
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Nil_Bundled) {
  ASSERT_TRUE([KeystoneGlue isValidSystemKeystone:@{} comparedToBundled:nil]);
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Empty_Bundled) {
  ASSERT_TRUE([KeystoneGlue isValidSystemKeystone:@{} comparedToBundled:@{}]);
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Bad_Bundled) {
  ASSERT_TRUE(
      [KeystoneGlue isValidSystemKeystone:@{} comparedToBundled:@{
        @[] : @2
      }]);
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Bad_Bundled_Version) {
  ASSERT_TRUE([KeystoneGlue isValidSystemKeystone:@{}
      comparedToBundled:@{
        @"CFBundleVersion" : @1
      }]);
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Bad_Bundled_Version_String) {
  ASSERT_TRUE([KeystoneGlue
      isValidSystemKeystone:@{}
          comparedToBundled:@{@"CFBundleVersion" : @"Hi how are you?"}]);
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Nil_System_Keystone) {
  ASSERT_FALSE([KeystoneGlue
      isValidSystemKeystone:nil
          comparedToBundled:@{@"CFBundleVersion" : @"1.2.3"}]);
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Empty_System_Keystone) {
  ASSERT_FALSE([KeystoneGlue
      isValidSystemKeystone:@{}
          comparedToBundled:@{@"CFBundleVersion" : @"1.2.3"}]);
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Bad_System_Keystone) {
  ASSERT_FALSE([KeystoneGlue
      isValidSystemKeystone:@{@"foo" : @"bar"}
          comparedToBundled:@{@"CFBundleVersion" : @"1.2.3"}]);
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Bad_System_Keystone_Version) {
  ASSERT_FALSE([KeystoneGlue
      isValidSystemKeystone:@{
        @"CFBundleVersion" : @[]
      }
          comparedToBundled:@{@"CFBundleVersion" : @"1.2.3"}]);
}

TEST_F(KeystoneGlueTest,
       isValidSystemKeystone_Bad_System_Keystone_Version_String) {
  ASSERT_FALSE([KeystoneGlue
      isValidSystemKeystone:@{@"CFBundleVersion" : @"I am baddy."}
          comparedToBundled:@{@"CFBundleVersion" : @"1.2.3"}]);
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Bad_System_Keystone_Outdated) {
  ASSERT_FALSE([KeystoneGlue
      isValidSystemKeystone:@{@"CFBundleVersion" : @"1.2.2.15"}
          comparedToBundled:@{@"CFBundleVersion" : @"1.2.3"}]);
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Bad_System_Keystone_Same) {
  ASSERT_TRUE([KeystoneGlue
      isValidSystemKeystone:@{@"CFBundleVersion" : @"1.2.3.4"}
          comparedToBundled:@{@"CFBundleVersion" : @"1.2.3.4"}]);
}

TEST_F(KeystoneGlueTest, isValidSystemKeystone_Bad_System_Keystone_Newer) {
  ASSERT_TRUE([KeystoneGlue
      isValidSystemKeystone:@{@"CFBundleVersion" : @"1.2.4.1"}
          comparedToBundled:@{@"CFBundleVersion" : @"1.2.3.4"}]);
}

}  // namespace
