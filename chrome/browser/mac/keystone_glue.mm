// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/mac/keystone_glue.h"

#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <vector>

#include "base/bind.h"
#include "base/file_version_info.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/authorization_util.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#import "chrome/browser/mac/keystone_registration.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/version_info/version_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

namespace ksr = keystone_registration;

// Constants for the brand file (uses an external file so it can survive
// updates to Chrome.)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define kStableBrandFileName @"Google Chrome Brand.plist"
#define kCanaryBrandFileName @"Google Chrome Canary Brand.plist"
#elif BUILDFLAG(CHROMIUM_BRANDING)
#define kStableBrandFileName @"Chromium Brand.plist"
#define kCanaryBrandFileName @"Chromium Canary Brand.plist"
#else
#error Unknown branding
#endif

// These directories are hardcoded in Keystone promotion preflight and the
// Keystone install script, so NSSearchPathForDirectoriesInDomains isn't used
// since the scripts couldn't use anything like that.
NSString* kStableBrandUserFile = @"~/Library/Google/" kStableBrandFileName;
NSString* kStableBrandSystemFile = @"/Library/Google/" kStableBrandFileName;
NSString* kCanaryBrandUserFile = @"~/Library/Google/" kCanaryBrandFileName;
NSString* kCanaryBrandSystemFile = @"/Library/Google/" kCanaryBrandFileName;

NSString* UserBrandFilePath(version_info::Channel channel) {
  NSString* file = (channel == version_info::Channel::CANARY)
                       ? kCanaryBrandUserFile
                       : kStableBrandUserFile;
  return [file stringByStandardizingPath];
}

NSString* SystemBrandFilePath(version_info::Channel channel) {
  NSString* file = (channel == version_info::Channel::CANARY)
                       ? kCanaryBrandSystemFile
                       : kStableBrandSystemFile;
  return [file stringByStandardizingPath];
}

// Adaptor for scheduling an Objective-C method call in ThreadPool.
class PerformBridge : public base::RefCountedThreadSafe<PerformBridge> {
 public:

  // Call |sel| on |target| with |arg| in a WorkerPool thread.
  // |target| and |arg| are retained, |arg| may be |nil|.
  static void PostPerform(id target, SEL sel, id arg) {
    DCHECK(target);
    DCHECK(sel);

    scoped_refptr<PerformBridge> op = new PerformBridge(target, sel, arg);
    base::PostTask(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&PerformBridge::Run, op.get()));
  }

  // Convenience for the no-argument case.
  static void PostPerform(id target, SEL sel) {
    PostPerform(target, sel, nil);
  }

 private:
  // Allow RefCountedThreadSafe<> to delete.
  friend class base::RefCountedThreadSafe<PerformBridge>;

  PerformBridge(id target, SEL sel, id arg)
      : target_([target retain]),
        sel_(sel),
        arg_([arg retain]) {
  }

  ~PerformBridge() {}

  // Happens on a WorkerPool thread.
  void Run() {
    @autoreleasepool {
      [target_ performSelector:sel_ withObject:arg_];
    }
  }

  base::scoped_nsobject<id> target_;
  SEL sel_;
  base::scoped_nsobject<id> arg_;
};

}  // namespace

@interface KeystoneGlue (Private)

// Returns the path to the application's Info.plist file.  This returns the
// outer application bundle's Info.plist, not the framework's Info.plist.
- (NSString*)appInfoPlistPath;

// Returns a dictionary containing parameters to be used for a KSRegistration
// -registerWithParameters: or -promoteWithParameters:authorization: call.
- (NSDictionary*)keystoneParameters;

// Called when Keystone registration completes.
- (void)registrationComplete:(NSNotification*)notification;

// Called periodically to announce activity by pinging the Keystone server.
- (void)markActive:(NSTimer*)timer;

// Called when an update check or update installation is complete.  Posts the
// kAutoupdateStatusNotification notification to the default notification
// center.
- (void)updateStatus:(AutoupdateStatus)status
             version:(NSString*)version
               error:(NSString*)error;

// Returns the version of the currently-installed application on disk.
- (NSString*)currentlyInstalledVersion;

// These three methods are used to determine the version of the application
// currently installed on disk, compare that to the currently-running version,
// decide whether any updates have been installed, and call
// -updateStatus:version:error:.
//
// In order to check the version on disk, the installed application's
// Info.plist dictionary must be read; in order to see changes as updates are
// applied, the dictionary must be read each time, bypassing any caches such
// as the one that NSBundle might be maintaining.  Reading files can be a
// blocking operation, and blocking operations are to be avoided on the main
// thread.  I'm not quite sure what jank means, but I bet that a blocked main
// thread would cause some of it.
//
// -determineUpdateStatusAsync is called on the main thread to initiate the
// operation.  It performs initial set-up work that must be done on the main
// thread and arranges for -determineUpdateStatus to be called on a work queue
// thread managed by WorkerPool.
// -determineUpdateStatus then reads the Info.plist, gets the version from the
// CFBundleShortVersionString key, and performs
// -determineUpdateStatusForVersion: on the main thread.
// -determineUpdateStatusForVersion: does the actual comparison of the version
// on disk with the running version and calls -updateStatus:version:error: with
// the results of its analysis.
- (void)determineUpdateStatusAsync;
- (void)determineUpdateStatus;
- (void)determineUpdateStatusForVersion:(NSString*)version;

// Returns YES if registration_ is definitely on a system ticket.
- (BOOL)isSystemTicket;

// Returns YES if Keystone is definitely installed at the system level,
// determined by the presence of an executable ksadmin program at the expected
// system location.
- (BOOL)isSystemKeystone;

// Called when ticket promotion completes.
- (void)promotionComplete:(NSNotification*)notification;

// Changes the application's ownership and permissions so that all files are
// owned by root:wheel and all files and directories are writable only by
// root, but readable and executable as needed by everyone.
// -changePermissionsForPromotionAsync is called on the main thread by
// -promotionComplete.  That routine calls
// -changePermissionsForPromotionWithTool: on a work queue thread.  When done,
// -changePermissionsForPromotionComplete is called on the main thread.
- (void)changePermissionsForPromotionAsync;
- (void)changePermissionsForPromotionWithTool:(NSString*)toolPath;
- (void)changePermissionsForPromotionComplete;

// Returns the brand file path to use for Keystone.
- (NSString*)brandFilePath;

// YES if no update installation has succeeded since a binary diff patch
// installation failed. This signals the need to attempt a full installer
// which does not depend on applying a patch to existing files.
- (BOOL)wantsFullInstaller;

// Returns an NSString* suitable for appending to a Chrome Keystone tag value or
// tag key.  If a full installer (as opposed to a binary diff/delta patch) is
// required, the tag suffix will contain the string "-full". If no special
// treatment is required, the tag suffix will be an empty string.
- (NSString*)tagSuffix;

@end  // @interface KeystoneGlue (Private)

NSString* const kAutoupdateStatusNotification = @"AutoupdateStatusNotification";
NSString* const kAutoupdateStatusStatus = @"status";
NSString* const kAutoupdateStatusVersion = @"version";
NSString* const kAutoupdateStatusErrorMessages = @"errormessages";

namespace {

NSString* const kChannelKey = @"KSChannelID";
NSString* const kBrandKey = @"KSBrandID";
NSString* const kVersionKey = @"KSVersion";

}  // namespace

@implementation KeystoneGlue

+ (id)defaultKeystoneGlue {
  static bool sTriedCreatingDefaultKeystoneGlue = false;
  static KeystoneGlue* sDefaultKeystoneGlue = nil;  // leaked

  if (!sTriedCreatingDefaultKeystoneGlue) {
    sTriedCreatingDefaultKeystoneGlue = true;

    sDefaultKeystoneGlue = [[KeystoneGlue alloc] init];
    [sDefaultKeystoneGlue loadParameters];
    if (![sDefaultKeystoneGlue loadKeystoneRegistration]) {
      [sDefaultKeystoneGlue release];
      sDefaultKeystoneGlue = nil;
    }
  }
  return sDefaultKeystoneGlue;
}

- (id)init {
  if ((self = [super init])) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

    [center addObserver:self
               selector:@selector(registrationComplete:)
                   name:ksr::KSRegistrationDidCompleteNotification
                 object:nil];

    [center addObserver:self
               selector:@selector(promotionComplete:)
                   name:ksr::KSRegistrationPromotionDidCompleteNotification
                 object:nil];

    [center addObserver:self
               selector:@selector(checkForUpdateComplete:)
                   name:ksr::KSRegistrationCheckForUpdateNotification
                 object:nil];

    [center addObserver:self
               selector:@selector(installUpdateComplete:)
                   name:ksr::KSRegistrationStartUpdateNotification
                 object:nil];

    // Set up the watcher for the staging key, for new-style updating. Use a
    // long polling time, as this isn't user-blocking, and it doesn't poll on
    // >=10.12 anyway.
    const NSTimeInterval kPollingTime = 60 * 60;  // 1 hour
    stagingKeyWatcher_.reset(
        [[CrStagingKeyWatcher alloc] initWithPollingTime:kPollingTime]);
    [stagingKeyWatcher_ setStagingKeyChangedObserver:^(BOOL stagingKeySet) {
      if (stagingKeySet) {
        // If the staging key is set, then there is a process waiting for Chrome
        // to quit, to allow it to switch out the binary on disk. Because
        // there's nothing for Chrome to do here except restart to allow the
        // installation, use |kAutoupdateInstalled| to ask the user to restart.
        [self updateStatus:kAutoupdateInstalled version:nil error:nil];
      }
    }];
  }

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (NSDictionary*)infoDictionary {
  // Use base::mac::OuterBundle() to get the Chrome app's own bundle identifier
  // and path, not the framework's.  For auto-update, the application is
  // what's significant here: it's used to locate the outermost part of the
  // application for the existence checker and other operations that need to
  // see the entire application bundle.
  return [base::mac::OuterBundle() infoDictionary];
}

- (void)loadParameters {
  NSBundle* appBundle = base::mac::OuterBundle();
  NSDictionary* infoDictionary = [self infoDictionary];

  NSString* productID = base::mac::ObjCCast<NSString>(
      [infoDictionary objectForKey:@"KSProductID"]);
  if (productID == nil) {
    productID = [appBundle bundleIdentifier];
  }

  NSString* appPath = [appBundle bundlePath];
  NSString* url = base::mac::ObjCCast<NSString>(
      [infoDictionary objectForKey:@"KSUpdateURL"]);
  NSString* version = base::mac::ObjCCast<NSString>(
      [infoDictionary objectForKey:kVersionKey]);

  if (!productID || !appPath || !url || !version) {
    // If parameters required for Keystone are missing, don't use it.
    return;
  }

  std::string channel = chrome::GetChannelName();
  // The stable channel has no tag.  If updating to stable, remove the
  // dev and beta tags since we've been "promoted".
  version_info::Channel channelType = chrome::GetChannelByName(channel);
  if (channelType == version_info::Channel::STABLE) {
    channel = base::SysNSStringToUTF8(ksr::KSRegistrationRemoveExistingTag);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    DCHECK(chrome::GetChannelByName(channel) == version_info::Channel::STABLE)
        << "-channel name modification has side effect";
#endif
  }

  productID_.reset([productID copy]);
  appPath_.reset([appPath copy]);
  url_.reset([url copy]);
  version_.reset([version copy]);
  channel_ = channel;
}

- (NSString*)brandFilePath {
  DCHECK(version_ != nil) << "-loadParameters must be called first";

  if (brandFile_)
    return brandFile_;

  NSFileManager* fm = [NSFileManager defaultManager];
  version_info::Channel channel = chrome::GetChannelByName(channel_);
  NSString* userBrandFile = UserBrandFilePath(channel);
  NSString* systemBrandFile = SystemBrandFilePath(channel);

  // Default to none.
  brandFile_.reset(@"", base::scoped_policy::RETAIN);

  // Only the stable and canary channel can have independent brand codes.

  if (channel == version_info::Channel::DEV ||
      channel == version_info::Channel::BETA) {
    // If on the dev or beta channel, this installation may have replaced
    // an older system-level installation. Check for a user brand file and
    // nuke it if present. Don't try to remove the system brand file, there
    // wouldn't be any permission to do so.
    //
    // Don't do this on the canary channel. The canary can run side-by-side
    // with another Google Chrome installation whose brand code, if any,
    // should remain intact.

    if ([fm fileExistsAtPath:userBrandFile]) {
      [fm removeItemAtPath:userBrandFile error:NULL];
    }

  } else if (channel == version_info::Channel::STABLE ||
             channel == version_info::Channel::CANARY) {
    // Stable and Canary use different app ids, so they can both have brand
    // codes. Even if Canary does not actively use brand codes, we want to
    // exercise the same logic, so that we can detect perf regressions early.

    // If there is a system brand file, use it.
    if ([fm fileExistsAtPath:systemBrandFile]) {
      // System

      // Use the system file that is there.
      brandFile_.reset(systemBrandFile, base::scoped_policy::RETAIN);

      // Clean up any old user level file.
      if ([fm fileExistsAtPath:userBrandFile]) {
        [fm removeItemAtPath:userBrandFile error:NULL];
      }

    } else {
      // User

      NSDictionary* infoDictionary = [self infoDictionary];
      NSString* appBundleBrandID = base::mac::ObjCCast<NSString>(
          [infoDictionary objectForKey:kBrandKey]);

      NSString* storedBrandID = nil;
      if ([fm fileExistsAtPath:userBrandFile]) {
        NSDictionary* storedBrandDict =
            [NSDictionary dictionaryWithContentsOfFile:userBrandFile];
        storedBrandID = base::mac::ObjCCast<NSString>(
            [storedBrandDict objectForKey:kBrandKey]);
      }

      if ((appBundleBrandID != nil) &&
          (![storedBrandID isEqualTo:appBundleBrandID])) {
        // App and store don't match, update store and use it.
        NSDictionary* storedBrandDict =
            [NSDictionary dictionaryWithObject:appBundleBrandID
                                        forKey:kBrandKey];
        // If Keystone hasn't been installed yet, the location the brand file
        // is written to won't exist, so manually create the directory.
        NSString* userBrandFileDirectory =
            [userBrandFile stringByDeletingLastPathComponent];
        if (![fm fileExistsAtPath:userBrandFileDirectory]) {
          if (![fm createDirectoryAtPath:userBrandFileDirectory
                  withIntermediateDirectories:YES
                                   attributes:nil
                                        error:NULL]) {
            LOG(ERROR) << "Failed to create the directory for the brand file";
          }
        }
        if ([storedBrandDict writeToFile:userBrandFile atomically:YES]) {
          brandFile_.reset(userBrandFile, base::scoped_policy::RETAIN);
        }
      } else if (storedBrandID) {
        // Had stored brand, use it.
        brandFile_.reset(userBrandFile, base::scoped_policy::RETAIN);
      }
    }
  }

  return brandFile_;
}

- (BOOL)loadKeystoneRegistration {
  if (!productID_ || !appPath_ || !url_ || !version_)
    return NO;

  // Load the KeystoneRegistration framework bundle if present.  It lives
  // inside the framework, so use base::mac::FrameworkBundle();
  NSString* ksrPath =
      [[base::mac::FrameworkBundle() privateFrameworksPath]
          stringByAppendingPathComponent:@"KeystoneRegistration.framework"];
  NSBundle* ksrBundle = [NSBundle bundleWithPath:ksrPath];
  [ksrBundle load];

  // Harness the KSRegistration class.
  Class ksrClass = [ksrBundle classNamed:@"KSRegistration"];
  KSRegistration* ksr = [ksrClass registrationWithProductID:productID_];
  if (!ksr)
    return NO;

  registration_.reset([ksr retain]);
  ksUnsignedReportingAttributeClass_ =
      [ksrBundle classNamed:@"KSUnsignedReportingAttribute"];
  return YES;
}

- (NSString*)appInfoPlistPath {
  // NSBundle ought to have a way to access this path directly, but it
  // doesn't.
  return [[appPath_ stringByAppendingPathComponent:@"Contents"]
             stringByAppendingPathComponent:@"Info.plist"];
}

- (NSDictionary*)keystoneParameters {
  NSNumber* xcType = [NSNumber numberWithInt:ksr::kKSPathExistenceChecker];
  NSNumber* preserveTTToken = [NSNumber numberWithBool:YES];
  NSString* appInfoPlistPath = [self appInfoPlistPath];
  NSString* brandKey = kBrandKey;
  NSString* brandPath = [self brandFilePath];

  if ([brandPath length] == 0) {
    // Brand path and brand key must be cleared together or ksadmin seems
    // to throw an error.
    brandKey = @"";
  }

  // Note that channel_ is permitted to be an empty string, but it must not be
  // nil.
  NSString* tagSuffix = [self tagSuffix];
  NSString* tagValue =
      [NSString stringWithFormat:@"%s%@", channel_.c_str(), tagSuffix];
  NSString* tagKey = [kChannelKey stringByAppendingString:tagSuffix];

  return @{
    ksr::KSRegistrationVersionKey : version_,
    ksr::KSRegistrationVersionPathKey : appInfoPlistPath,
    ksr::KSRegistrationVersionKeyKey : kVersionKey,
    ksr::KSRegistrationExistenceCheckerTypeKey : xcType,
    ksr::KSRegistrationExistenceCheckerStringKey : appPath_.get(),
    ksr::KSRegistrationServerURLStringKey : url_.get(),
    ksr::KSRegistrationPreserveTrustedTesterTokenKey : preserveTTToken,
    ksr::KSRegistrationTagKey : tagValue,
    ksr::KSRegistrationTagPathKey : appInfoPlistPath,
    ksr::KSRegistrationTagKeyKey : tagKey,
    ksr::KSRegistrationBrandPathKey : brandPath,
    ksr::KSRegistrationBrandKeyKey : brandKey
  };
}

- (void)setRegistrationActive {
  DCHECK(registration_);
  registrationActive_ = YES;
  NSError* setActiveError = nil;
  if (![registration_ setActiveWithError:&setActiveError]) {
    VLOG(1) << [setActiveError localizedDescription];
  }
}

- (void)registerWithKeystone {
  DCHECK(registration_);

  [self updateStatus:kAutoupdateRegistering version:nil error:nil];

  NSDictionary* parameters = [self keystoneParameters];
  BOOL result = [registration_ registerWithParameters:parameters];
  if (!result) {
    // TODO: If Keystone ever makes a variant of this API with a withError:
    // parameter, include the error message here in the call to updateStatus:.
    [self updateStatus:kAutoupdateRegisterFailed version:nil error:nil];
    return;
  }

  // Upon completion, ksr::KSRegistrationDidCompleteNotification will be
  // posted, and -registrationComplete: will be called.

  // Set up hourly activity pings.
  timer_ = [NSTimer scheduledTimerWithTimeInterval:60 * 60  // One hour
                                            target:self
                                          selector:@selector(markActive:)
                                          userInfo:nil
                                           repeats:YES];
}

- (BOOL)isRegisteredAndActive {
  return registrationActive_;
}

- (void)registrationComplete:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  NSNumber* status = base::mac::ObjCCast<NSNumber>(
     [userInfo objectForKey:ksr::KSRegistrationStatusKey]);
  NSString* errorMessages = base::mac::ObjCCast<NSString>(
     [userInfo objectForKey:ksr::KSRegistrationUpdateCheckRawErrorMessagesKey]);

  if ([status boolValue]) {
    if ([self needsPromotion]) {
      [self updateStatus:kAutoupdateNeedsPromotion
                 version:nil
                   error:errorMessages];
    } else {
      [self updateStatus:kAutoupdateRegistered
                 version:nil
                   error:errorMessages];
    }
  } else {
    // Dump registration_?
    [self updateStatus:kAutoupdateRegisterFailed
               version:nil
                 error:errorMessages];
  }
}

- (void)stopTimer {
  [timer_ invalidate];
}

- (void)markActive:(NSTimer*)timer {
  [self setRegistrationActive];
}

- (void)checkForUpdate {
  DCHECK(registration_);

  if ([self asyncOperationPending]) {
    // Update check already in process; return without doing anything.
    return;
  }

  [self updateStatus:kAutoupdateChecking version:nil error:nil];

  // All checks from inside Chrome are considered user-initiated, because they
  // only happen following a user action, such as visiting the about page.
  // Non-user-initiated checks are the periodic checks automatically made by
  // Keystone, which don't come through this code path (or even this process).
  [registration_ checkForUpdateWasUserInitiated:YES];

  // Upon completion, ksr::KSRegistrationCheckForUpdateNotification will be
  // posted, and -checkForUpdateComplete: will be called.
}

- (void)checkForUpdateComplete:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  NSNumber* error = base::mac::ObjCCast<NSNumber>(
      [userInfo objectForKey:ksr::KSRegistrationUpdateCheckErrorKey]);
  NSNumber* status = base::mac::ObjCCast<NSNumber>(
      [userInfo objectForKey:ksr::KSRegistrationStatusKey]);
  NSString* errorMessages = base::mac::ObjCCast<NSString>(
     [userInfo objectForKey:ksr::KSRegistrationUpdateCheckRawErrorMessagesKey]);

  if ([error boolValue]) {
    [self updateStatus:kAutoupdateCheckFailed
               version:nil
                 error:errorMessages];
  } else if ([status boolValue]) {
    // If an update is known to be available, go straight to
    // -updateStatus:version:.  It doesn't matter what's currently on disk.
    NSString* version = base::mac::ObjCCast<NSString>(
        [userInfo objectForKey:ksr::KSRegistrationVersionKey]);
    [self updateStatus:kAutoupdateAvailable
               version:version
                 error:errorMessages];
  } else {
    // If no updates are available, check what's on disk, because an update
    // may have already been installed.  This check happens on another thread,
    // and -updateStatus:version: will be called on the main thread when done.
    [self determineUpdateStatusAsync];
  }
}

- (void)installUpdate {
  DCHECK(registration_);

  if ([self asyncOperationPending]) {
    // Update check already in process; return without doing anything.
    return;
  }

  [self updateStatus:kAutoupdateInstalling version:nil error:nil];

  [registration_ startUpdate];

  // Upon completion, ksr::KSRegistrationStartUpdateNotification will be
  // posted, and -installUpdateComplete: will be called.
}

- (void)installUpdateComplete:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  NSNumber* successfulInstall = base::mac::ObjCCast<NSNumber>(
      [userInfo objectForKey:ksr::KSUpdateCheckSuccessfullyInstalledKey]);
  NSString* errorMessages = base::mac::ObjCCast<NSString>(
     [userInfo objectForKey:ksr::KSRegistrationUpdateCheckRawErrorMessagesKey]);

  // http://crbug.com/160308 and b/7517358: when using system Keystone and on
  // a user ticket, KSUpdateCheckSuccessfulKey will be NO even when an update
  // was installed correctly, so don't check it. It should be redudnant when
  // KSUpdateCheckSuccessfullyInstalledKey is checked.
  if (![successfulInstall intValue]) {
    [self updateStatus:kAutoupdateInstallFailed
               version:nil
                 error:errorMessages];
  } else {
    updateSuccessfullyInstalled_ = YES;

    // Nothing in the notification dictionary reports the version that was
    // installed.  Figure it out based on what's on disk.
    [self determineUpdateStatusAsync];
  }
}

- (NSString*)currentlyInstalledVersion {
  NSString* appInfoPlistPath = [self appInfoPlistPath];
  NSDictionary* infoPlist =
      [NSDictionary dictionaryWithContentsOfFile:appInfoPlistPath];
  return base::mac::ObjCCast<NSString>(
      [infoPlist objectForKey:@"CFBundleShortVersionString"]);
}

// Runs on the main thread.
- (void)determineUpdateStatusAsync {
  DCHECK([NSThread isMainThread]);

  PerformBridge::PostPerform(self, @selector(determineUpdateStatus));
}

// Runs on a thread managed by WorkerPool.
- (void)determineUpdateStatus {
  DCHECK(![NSThread isMainThread]);

  NSString* version = [self currentlyInstalledVersion];

  [self performSelectorOnMainThread:@selector(determineUpdateStatusForVersion:)
                         withObject:version
                      waitUntilDone:NO];
}

// Runs on the main thread.
- (void)determineUpdateStatusForVersion:(NSString*)version {
  DCHECK([NSThread isMainThread]);

  AutoupdateStatus status;
  if (updateSuccessfullyInstalled_) {
    // If an update was successfully installed and this object saw it happen,
    // then don't even bother comparing versions.
    status = kAutoupdateInstalled;
  } else if ([stagingKeyWatcher_ isStagingKeySet]) {
    // If there's a staging key, then the update will happen on restart.
    status = kAutoupdateInstalled;
  } else {
    NSString* currentVersion = base::SysUTF8ToNSString(chrome::kChromeVersion);
    if (!version) {
      // If the version on disk could not be determined, assume that
      // whatever's running is current.
      version = currentVersion;
      status = kAutoupdateCurrent;
    } else if ([version isEqualToString:currentVersion]) {
      status = kAutoupdateCurrent;
    } else {
      // If the version on disk doesn't match what's currently running, an
      // update must have been applied in the background, without this app's
      // direct participation.  Leave updateSuccessfullyInstalled_ alone
      // because there's no direct knowledge of what actually happened.
      status = kAutoupdateInstalled;
    }
  }

  [self updateStatus:status version:version error:nil];
}

- (void)updateStatus:(AutoupdateStatus)status
             version:(NSString*)version
               error:(NSString*)error {
  NSNumber* statusNumber = [NSNumber numberWithInt:status];
  NSMutableDictionary* dictionary =
      [NSMutableDictionary dictionaryWithObject:statusNumber
                                         forKey:kAutoupdateStatusStatus];
  if ([version length]) {
    [dictionary setObject:version forKey:kAutoupdateStatusVersion];
  }
  if ([error length]) {
    [dictionary setObject:error forKey:kAutoupdateStatusErrorMessages];
  }

  NSNotification* notification =
      [NSNotification notificationWithName:kAutoupdateStatusNotification
                                    object:self
                                  userInfo:dictionary];
  recentNotification_.reset([notification retain]);

  [[NSNotificationCenter defaultCenter] postNotification:notification];
}

- (NSNotification*)recentNotification {
  return [[recentNotification_ retain] autorelease];
}

- (AutoupdateStatus)recentStatus {
  NSDictionary* dictionary = [recentNotification_ userInfo];
  NSNumber* status = base::mac::ObjCCastStrict<NSNumber>(
      [dictionary objectForKey:kAutoupdateStatusStatus]);
  return static_cast<AutoupdateStatus>([status intValue]);
}

- (BOOL)asyncOperationPending {
  AutoupdateStatus status = [self recentStatus];
  return status == kAutoupdateRegistering ||
         status == kAutoupdateChecking ||
         status == kAutoupdateInstalling ||
         status == kAutoupdatePromoting;
}

- (BOOL)isSystemTicket {
  DCHECK(registration_);
  return [registration_ ticketType] == ksr::kKSRegistrationSystemTicket;
}

- (BOOL)isSystemKeystone {
  struct stat statbuf;
  if (stat("/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/"
           "Contents/MacOS/ksadmin",
           &statbuf) != 0) {
    return NO;
  }

  if (!(statbuf.st_mode & S_IXUSR)) {
    return NO;
  }

  return YES;
}

- (BOOL)isOnReadOnlyFilesystem {
  const char* appPathC = [appPath_ fileSystemRepresentation];
  struct statfs statfsBuf;

  if (statfs(appPathC, &statfsBuf) != 0) {
    PLOG(ERROR) << "statfs";
    // Be optimistic about the filesystem's writability.
    return NO;
  }

  return (statfsBuf.f_flags & MNT_RDONLY) != 0;
}

- (BOOL)isAutoupdateEnabledForAllUsers {
  return [self isSystemKeystone] && [self isSystemTicket];
}

// Compares the version of the installed system Keystone to the version of
// KeystoneRegistration.framework. The method is a class method, so that
// tests can pick it up.
+ (BOOL)isValidSystemKeystone:(NSDictionary*)systemKeystonePlistContents
            comparedToBundled:(NSDictionary*)bundledKeystonePlistContents {
  NSString* versionKey = base::mac::CFToNSCast(kCFBundleVersionKey);

  // If the bundled version is missing or broken, this question is irrelevant.
  NSString* bundledKeystoneVersionString =
      base::mac::ObjCCast<NSString>(bundledKeystonePlistContents[versionKey]);
  if (!bundledKeystoneVersionString.length)
    return YES;
  base::Version bundled_version(
      base::SysNSStringToUTF8(bundledKeystoneVersionString));
  if (!bundled_version.IsValid())
    return YES;

  NSString* systemKeystoneVersionString =
      base::mac::ObjCCast<NSString>(systemKeystonePlistContents[versionKey]);
  if (!systemKeystoneVersionString.length)
    return NO;

  // Installed Keystone's version should always be >= than the bundled one.
  base::Version system_version(
      base::SysNSStringToUTF8(systemKeystoneVersionString));
  if (!system_version.IsValid() || system_version < bundled_version)
    return NO;

  return YES;
}

- (BOOL)isSystemKeystoneBroken {
  DCHECK([self isSystemKeystone])
      << "Call this method only for system Keystone.";

  NSDictionary* systemKeystonePlist =
      [NSDictionary dictionaryWithContentsOfFile:
                        @"/Library/Google/GoogleSoftwareUpdate/"
                        @"GoogleSoftwareUpdate.bundle/Contents/Info.plist"];
  NSBundle* keystoneFramework = [NSBundle bundleForClass:[registration_ class]];
  return ![[self class] isValidSystemKeystone:systemKeystonePlist
                            comparedToBundled:keystoneFramework.infoDictionary];
}

- (BOOL)needsPromotion {
  // Don't promote when on a read-only filesystem.
  if ([self isOnReadOnlyFilesystem]) {
    return NO;
  }

  BOOL isSystemKeystone = [self isSystemKeystone];
  if (isSystemKeystone) {
    // We can recover broken user keystone, but not broken system one.
    if ([self isSystemKeystoneBroken])
      return YES;
  }

  // System ticket requires system Keystone for the updates to work.
  if ([self isSystemTicket])
    return !isSystemKeystone;

  // Check the outermost bundle directory, the main executable path, and the
  // framework directory.  It may be enough to just look at the outermost
  // bundle directory, but checking an interior file and directory can be
  // helpful in case permissions are set differently only on the outermost
  // directory.  An interior file and directory are both checked because some
  // file operations, such as Snow Leopard's Finder's copy operation when
  // authenticating, may actually result in different ownership being applied
  // to files and directories.
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSString* executablePath = [base::mac::OuterBundle() executablePath];
  NSString* frameworkPath = [base::mac::FrameworkBundle() bundlePath];
  return ![fileManager isWritableFileAtPath:appPath_] ||
         ![fileManager isWritableFileAtPath:executablePath] ||
         ![fileManager isWritableFileAtPath:frameworkPath];
}

- (BOOL)wantsPromotion {
  if ([self needsPromotion]) {
    return YES;
  }

  // These are the same unpromotable cases as in -needsPromotion.
  if ([self isOnReadOnlyFilesystem] || [self isSystemTicket]) {
    return NO;
  }

  return [appPath_ hasPrefix:@"/Applications/"];
}

- (void)promoteTicket {
  if ([self asyncOperationPending] || ![self wantsPromotion]) {
    // Because there are multiple ways of reaching promoteTicket that might
    // not lock each other out, it may be possible to arrive here while an
    // asynchronous operation is pending, or even after promotion has already
    // occurred.  Just quietly return without doing anything.
    return;
  }

  NSString* prompt = l10n_util::GetNSStringFWithFixup(
      IDS_PROMOTE_AUTHENTICATION_PROMPT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  base::mac::ScopedAuthorizationRef authorization(
      base::mac::AuthorizationCreateToRunAsRoot(
          base::mac::NSToCFCast(prompt)));
  if (!authorization.get()) {
    return;
  }

  [self promoteTicketWithAuthorization:authorization.release() synchronous:NO];
}

- (void)promoteTicketWithAuthorization:(AuthorizationRef)anAuthorization
                           synchronous:(BOOL)synchronous {
  DCHECK(registration_);

  base::mac::ScopedAuthorizationRef authorization(anAuthorization);
  anAuthorization = nullptr;

  if ([self asyncOperationPending]) {
    // Starting a synchronous operation while an asynchronous one is pending
    // could be trouble.
    return;
  }
  if (!synchronous && ![self wantsPromotion]) {
    // If operating synchronously, the call came from the installer, which
    // means that a system ticket is required.  Otherwise, only allow
    // promotion if it's wanted.
    return;
  }

  synchronousPromotion_ = synchronous;

  [self updateStatus:kAutoupdatePromoting version:nil error:nil];

  // TODO(mark): Remove when able!
  //
  // keystone_promote_preflight will copy the current brand information out to
  // the system level so all users can share the data as part of the ticket
  // promotion.
  //
  // It will also ensure that the Keystone system ticket store is in a usable
  // state for all users on the system.  Ideally, Keystone's installer or
  // another part of Keystone would handle this.  The underlying problem is
  // http://b/2285921, and it causes http://b/2289908, which this workaround
  // addresses.
  //
  // This is run synchronously, which isn't optimal, but
  // -[KSRegistration promoteWithParameters:authorization:] is currently
  // synchronous too, and this operation needs to happen before that one.
  //
  // TODO(mark): Make asynchronous.  That only makes sense if the promotion
  // operation itself is asynchronous too.  http://b/2290009.  Hopefully,
  // the Keystone promotion code will just be changed to do what preflight
  // now does, and then the preflight script can be removed instead.
  // However, preflight operation (and promotion) should only be asynchronous
  // if the synchronous parameter is NO.
  NSString* preflightPath =
      [base::mac::FrameworkBundle()
          pathForResource:@"keystone_promote_preflight"
                   ofType:@"sh"];
  const char* preflightPathC = [preflightPath fileSystemRepresentation];

  // This is typically a once per machine operation, so it is not worth caching
  // the type of brand file (user vs system). Figure it out here:
  version_info::Channel channel = chrome::GetChannelByName(channel_);
  NSString* userBrandFile = UserBrandFilePath(channel);
  NSString* systemBrandFile = SystemBrandFilePath(channel);
  const char* arguments[] = {NULL, NULL, NULL};
  BOOL userBrand = NO;
  if ([brandFile_ isEqualToString:userBrandFile]) {
    // Running with user level brand file, promote to the system level.
    userBrand = YES;
    arguments[0] = userBrandFile.UTF8String;
    arguments[1] = systemBrandFile.UTF8String;
  }

  int exit_status;
  OSStatus status = base::mac::ExecuteWithPrivilegesAndWait(
      authorization,
      preflightPathC,
      kAuthorizationFlagDefaults,
      arguments,
      NULL,  // pipe
      &exit_status);
  if (status != errAuthorizationSuccess) {
    // It's possible to get an OS-provided error string for this return code
    // using base::mac::DescriptionFromOSStatus, but most of those strings are
    // not useful/actionable for users, so we stick with the error code instead.
    NSString* errorMessage = l10n_util::GetNSStringFWithFixup(
        IDS_PROMOTE_PREFLIGHT_LAUNCH_ERROR, base::NumberToString16(status));
    [self updateStatus:kAutoupdatePromoteFailed
               version:nil
                 error:errorMessage];
    return;
  }
  if (exit_status != 0) {
    NSString* errorMessage = l10n_util::GetNSStringFWithFixup(
        IDS_PROMOTE_PREFLIGHT_SCRIPT_ERROR, base::NumberToString16(status));
    [self updateStatus:kAutoupdatePromoteFailed
               version:nil
                 error:errorMessage];
    return;
  }

  // Hang on to the AuthorizationRef so that it can be used once promotion is
  // complete.  Do this before asking Keystone to promote the ticket, because
  // -promotionComplete: may be called from inside the Keystone promotion
  // call.
  authorization_.swap(authorization);

  NSDictionary* parameters = [self keystoneParameters];

  // If the brand file is user level, update parameters to point to the new
  // system level file during promotion.
  if (userBrand) {
    NSMutableDictionary* tempParameters =
        [[parameters mutableCopy] autorelease];
    tempParameters[ksr::KSRegistrationBrandPathKey] = systemBrandFile;
    brandFile_.reset(systemBrandFile, base::scoped_policy::RETAIN);
    parameters = tempParameters;
  }

  if (![registration_ promoteWithParameters:parameters
                              authorization:authorization_]) {
    // TODO: If Keystone ever makes a variant of this API with a withError:
    // parameter, include the error message here in the call to updateStatus:.
    [self updateStatus:kAutoupdatePromoteFailed version:nil error:nil];
    authorization_.reset();
    return;
  }

  // Upon completion, ksr::KSRegistrationPromotionDidCompleteNotification will
  // be posted, and -promotionComplete: will be called.

  // If synchronous, see to it that this happens immediately. Give it a
  // 10-second deadline.
  if (synchronous) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 10, false);
  }
}

- (void)promotionComplete:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  NSNumber* status = base::mac::ObjCCast<NSNumber>(
      [userInfo objectForKey:ksr::KSRegistrationStatusKey]);

  if ([status boolValue]) {
    if (synchronousPromotion_) {
      // Short-circuit: if performing a synchronous promotion, the promotion
      // came from the installer, which already set the permissions properly.
      // Rather than run a duplicate permission-changing operation, jump
      // straight to "done."
      [self changePermissionsForPromotionComplete];
    } else {
      [self changePermissionsForPromotionAsync];
    }
  } else {
    authorization_.reset();
    [self updateStatus:kAutoupdatePromoteFailed version:nil error:nil];
  }

  if (synchronousPromotion_) {
    // The run loop doesn't need to wait for this any longer.
    CFRunLoopRef runLoop = CFRunLoopGetCurrent();
    CFRunLoopStop(runLoop);
    CFRunLoopWakeUp(runLoop);
  }
}

- (void)changePermissionsForPromotionAsync {
  // NSBundle is not documented as being thread-safe.  Do NSBundle operations
  // on the main thread before jumping over to a WorkerPool-managed
  // thread to run the tool.
  DCHECK([NSThread isMainThread]);

  SEL selector = @selector(changePermissionsForPromotionWithTool:);
  NSString* toolPath =
      [base::mac::FrameworkBundle()
          pathForResource:@"keystone_promote_postflight"
                   ofType:@"sh"];

  PerformBridge::PostPerform(self, selector, toolPath);
}

- (void)changePermissionsForPromotionWithTool:(NSString*)toolPath {
  const char* toolPathC = [toolPath fileSystemRepresentation];

  const char* appPathC = [appPath_ fileSystemRepresentation];
  const char* arguments[] = {appPathC, NULL};

  int exit_status;
  OSStatus status = base::mac::ExecuteWithPrivilegesAndWait(
      authorization_,
      toolPathC,
      kAuthorizationFlagDefaults,
      arguments,
      NULL,  // pipe
      &exit_status);
  if (status != errAuthorizationSuccess) {
    OSSTATUS_LOG(ERROR, status)
        << "AuthorizationExecuteWithPrivileges postflight";
  } else if (exit_status != 0) {
    LOG(ERROR) << "keystone_promote_postflight status " << exit_status;
  }

  SEL selector = @selector(changePermissionsForPromotionComplete);
  [self performSelectorOnMainThread:selector
                         withObject:nil
                      waitUntilDone:NO];
}

- (void)changePermissionsForPromotionComplete {
  authorization_.reset();

  [self updateStatus:kAutoupdatePromoted version:nil error:nil];
}

- (void)setAppPath:(NSString*)appPath {
  if (appPath != appPath_) {
    appPath_.reset([appPath copy]);
  }
}

- (BOOL)wantsFullInstaller {
  // It's difficult to check the tag prior to Keystone registration, and
  // performing registration replaces the tag. keystone_install.sh
  // communicates a need for a full installer with Chrome in this file,
  // .want_full_installer.
  NSString* wantFullInstallerPath =
      [appPath_ stringByAppendingPathComponent:@".want_full_installer"];
  NSString* wantFullInstallerContents =
      [NSString stringWithContentsOfFile:wantFullInstallerPath
                                encoding:NSUTF8StringEncoding
                                   error:NULL];
  if (!wantFullInstallerContents) {
    return NO;
  }

  NSString* wantFullInstallerVersion =
      [wantFullInstallerContents stringByTrimmingCharactersInSet:
          [NSCharacterSet newlineCharacterSet]];
  return [wantFullInstallerVersion isEqualToString:version_];
}

- (NSString*)tagSuffix {
  // Tag suffix components are not entirely arbitrary: all possible tag keys
  // must be present in the application's Info.plist, there must be
  // server-side agreement on the processing and meaning of tag suffix
  // components, and other code that manipulates tag values (such as the
  // Keystone update installation script) must be tag suffix-aware. To reduce
  // the number of tag suffix combinations that need to be listed in
  // Info.plist, tag suffix components should only be appended to the tag
  // suffix in ASCII sort order.
  NSString* tagSuffix = @"";
  if ([self wantsFullInstaller]) {
    tagSuffix = [tagSuffix stringByAppendingString:@"-full"];
  }
  return tagSuffix;
}

@end  // @implementation KeystoneGlue

namespace {

std::string BrandCodeInternal() {
  KeystoneGlue* keystone_glue = [KeystoneGlue defaultKeystoneGlue];
  NSString* brand_path = [keystone_glue brandFilePath];

  if (![brand_path length])
    return std::string();

  NSDictionary* dict =
      [NSDictionary dictionaryWithContentsOfFile:brand_path];
  NSString* brand_code =
      base::mac::ObjCCast<NSString>([dict objectForKey:kBrandKey]);
  if (brand_code)
    return base::SysNSStringToUTF8(brand_code);

  return std::string();
}

}  // namespace

namespace keystone_glue {

std::string BrandCode() {
  // |s_brand_code| is leaked.
  static std::string* s_brand_code = new std::string(BrandCodeInternal());
  return *s_brand_code;
}

bool KeystoneEnabled() {
  return [KeystoneGlue defaultKeystoneGlue] != nil;
}

base::string16 CurrentlyInstalledVersion() {
  KeystoneGlue* keystoneGlue = [KeystoneGlue defaultKeystoneGlue];
  NSString* version = [keystoneGlue currentlyInstalledVersion];
  return base::SysNSStringToUTF16(version);
}

}  // namespace keystone_glue
