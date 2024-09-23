// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/mac/install_from_dmg.h"

#import <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/IOKitLib.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <unistd.h>

#include <algorithm>

#include "base/apple/bridging.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/mach_logging.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/authorization_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_authorizationref.h"
#include "base/mac/scoped_ioobject.h"
#include "base/memory/scoped_policy.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/mac/dock.h"
#include "chrome/browser/mac/relauncher.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Given an io_service_t (expected to be of class IOMedia), walks the ancestor
// chain, returning the closest ancestor that implements the specified class,
// if any. If no such ancestor is found, returns IO_OBJECT_NULL. Following
// the "copy" rule, the caller assumes ownership of the returned value.
//
// Note that this looks for the class by conformance, not equality. The reason
// for that is that for IOHDIXHDDrive the actual classes found will be
// IOHDIXHDDriveOutKernel for disk images mounted "out-of-kernel" or
// IOHDIXHDDriveInKernel for disk images mounted "in-kernel." (See the
// documentation for "hdiutil attach -kernel" for more information on the
// distinction.)
base::mac::ScopedIOObject<io_service_t> GetDiskImageAncestorForMedia(
    const char* disk_image_class,
    base::mac::ScopedIOObject<io_service_t> media) {
  // This is highly unlikely. media as passed in is expected to be of class
  // IOMedia. Since the media service's entire ancestor chain will be checked,
  // though, check it as well.
  if (IOObjectConformsTo(media.get(), disk_image_class)) {
    return media;
  }

  io_iterator_t iterator_ref;
  kern_return_t kr = IORegistryEntryCreateIterator(
      media.get(), kIOServicePlane,
      kIORegistryIterateRecursively | kIORegistryIterateParents, &iterator_ref);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "IORegistryEntryCreateIterator";
    return base::mac::ScopedIOObject<io_service_t>();
  }
  base::mac::ScopedIOObject<io_iterator_t> iterator(iterator_ref);
  iterator_ref = IO_OBJECT_NULL;

  // Look at each of the ancestor services, beginning with the parent,
  // iterating all the way up to the device tree's root. If any ancestor
  // service matches the class used for disk images, the media resides on a
  // disk image, and the disk image file's path can be determined by examining
  // the image-path property.
  for (base::mac::ScopedIOObject<io_service_t> ancestor(
           IOIteratorNext(iterator.get()));
       ancestor; ancestor.reset(IOIteratorNext(iterator.get()))) {
    if (IOObjectConformsTo(ancestor.get(), disk_image_class)) {
      return ancestor;
    }
  }

  // The media does not reside on a disk image.
  return base::mac::ScopedIOObject<io_service_t>();
}

// Given an io_service_t (expected to be of class IOMedia), determines whether
// that service is on a disk image. If it is, returns true. If image_path is
// present, it will be set to the pathname of the disk image file, encoded in
// filesystem encoding.
//
// There are two ways to do this using SPI: The first way would be to use
// DIHLCopyImageForVolume() from the DiskImages private framework.  The second
// way would be to use _kCFURLVolumeIsDiskImageKey and
// _kCFURLDiskImageBackingURLKey from CFURLPriv. However, because downstream
// users want to use Chromium as a base for code in the MAS, neither are used
// here. The request for a real API is FB9139935.
bool MediaResidesOnDiskImage(base::mac::ScopedIOObject<io_service_t> media,
                             std::string* image_path) {
  if (image_path) {
    image_path->clear();
  }

  if (base::mac::MacOSMajorVersion() >= 12) {
    // Starting with macOS 12 "Monterey", the IOMedia has an ancestor of
    // type "AppleDiskImageDevice" that has a property "DiskImageURL" of string
    // type.

    base::mac::ScopedIOObject<io_service_t> di_device =
        GetDiskImageAncestorForMedia("AppleDiskImageDevice", media);
    if (di_device) {
      if (image_path) {
        base::apple::ScopedCFTypeRef<CFTypeRef> disk_image_url_cftyperef(
            IORegistryEntryCreateCFProperty(di_device.get(),
                                            CFSTR("DiskImageURL"),
                                            /*allocator=*/nullptr,
                                            /*options=*/0));
        if (!disk_image_url_cftyperef) {
          LOG(ERROR)
              << "IORegistryEntryCreateCFProperty failed for DiskImageURL";
          return true;
        }

        CFStringRef disk_image_url_string =
            base::apple::CFCast<CFStringRef>(disk_image_url_cftyperef.get());
        if (!disk_image_url_string) {
          base::apple::ScopedCFTypeRef<CFStringRef> observed_type_cf(
              CFCopyTypeIDDescription(
                  CFGetTypeID(disk_image_url_cftyperef.get())));
          LOG(ERROR) << "DiskImageURL: expected CFString, observed "
                     << base::SysCFStringRefToUTF8(observed_type_cf.get());
          return true;
        }

        base::apple::ScopedCFTypeRef<CFURLRef> disk_image_url(
            CFURLCreateWithString(
                /*allocator=*/nullptr, disk_image_url_string,
                /*baseURL=*/nullptr));
        if (!disk_image_url) {
          LOG(ERROR) << "CFURLCreateWithString failed";
          return true;
        }

        base::apple::ScopedCFTypeRef<CFStringRef> disk_image_path(
            CFURLCopyFileSystemPath(disk_image_url.get(),
                                    kCFURLPOSIXPathStyle));
        if (!disk_image_path) {
          LOG(ERROR) << "CFURLCopyFileSystemPath failed";
          return true;
        }

        *image_path = base::SysCFStringRefToUTF8(disk_image_path.get());
      }

      return true;
    }
  } else {
    // From the mists of time through macOS 11 "Big Sur", the IOMedia has an
    // ancestor of type "IOHDIXHDDrive" that has a property "image-path" of data
    // type.

    base::mac::ScopedIOObject<io_service_t> hdix_drive =
        GetDiskImageAncestorForMedia("IOHDIXHDDrive", media);
    if (hdix_drive) {
      if (image_path) {
        base::apple::ScopedCFTypeRef<CFTypeRef> image_path_cftyperef(
            IORegistryEntryCreateCFProperty(hdix_drive.get(),
                                            CFSTR("image-path"),
                                            /*allocator=*/nullptr,
                                            /*options=*/0));
        if (!image_path_cftyperef) {
          LOG(ERROR) << "IORegistryEntryCreateCFProperty failed for image-path";
          return true;
        }

        CFDataRef image_path_data =
            base::apple::CFCast<CFDataRef>(image_path_cftyperef.get());
        if (!image_path_data) {
          base::apple::ScopedCFTypeRef<CFStringRef> observed_type_cf(
              CFCopyTypeIDDescription(CFGetTypeID(image_path_cftyperef.get())));
          LOG(ERROR) << "image-path: expected CFData, observed "
                     << base::SysCFStringRefToUTF8(observed_type_cf.get());
          return true;
        }

        CFIndex length = CFDataGetLength(image_path_data);
        if (length <= 0) {
          LOG(ERROR) << "image_path_data is unexpectedly empty";
          return true;
        }
        char* image_path_c = base::WriteInto(image_path, length + 1);
        CFDataGetBytes(image_path_data, CFRangeMake(0, length),
                       reinterpret_cast<UInt8*>(image_path_c));
      }

      return true;
    }
  }

  return false;
}

// Returns `DiskImageStatusTrue` if `path` is located on a read-only filesystem
// of a disk image, `DiskImageStatusFalse` if not, or `DiskImageStatusFailure`
// in the event of an error. If `out_dmg_bsd_device_name` is non-null, it will
// be set to the BSD device name for the disk image's device, in "diskNsM" form.
DiskImageStatus IsPathOnReadOnlyDiskImage(
    const char path[],
    std::string* out_dmg_bsd_device_name) {
  if (out_dmg_bsd_device_name) {
    out_dmg_bsd_device_name->clear();
  }

  struct statfs statfs_buf;
  if (statfs(path, &statfs_buf) != 0) {
    PLOG(ERROR) << "statfs " << path;
    return DiskImageStatusFailure;
  }

  if (!(statfs_buf.f_flags & MNT_RDONLY)) {
    // Not on a read-only filesystem.
    return DiskImageStatusFalse;
  }

  const char dev_root[] = "/dev/";
  const int dev_root_length = std::size(dev_root) - 1;
  if (strncmp(statfs_buf.f_mntfromname, dev_root, dev_root_length) != 0) {
    // Not rooted at dev_root, no BSD name to search on.
    return DiskImageStatusFalse;
  }

  // BSD names in IOKit don't include dev_root.
  const char* dmg_bsd_device_name = statfs_buf.f_mntfromname + dev_root_length;
  if (out_dmg_bsd_device_name) {
    out_dmg_bsd_device_name->assign(dmg_bsd_device_name);
  }

  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> match_dict(
      IOBSDNameMatching(kIOMasterPortDefault, /*options=*/0,
                        dmg_bsd_device_name));
  if (!match_dict) {
    LOG(ERROR) << "IOBSDNameMatching " << dmg_bsd_device_name;
    return DiskImageStatusFailure;
  }

  base::mac::ScopedIOObject<io_iterator_t> iterator;
  kern_return_t kr = IOServiceGetMatchingServices(
      kIOMasterPortDefault, match_dict.release(), iterator.InitializeInto());
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "IOServiceGetMatchingServices";
    return DiskImageStatusFailure;
  }

  // There needs to be exactly one matching service.
  base::mac::ScopedIOObject<io_service_t> media(IOIteratorNext(iterator.get()));
  if (!media) {
    LOG(ERROR) << "IOIteratorNext: no service";
    return DiskImageStatusFailure;
  }
  base::mac::ScopedIOObject<io_service_t> unexpected_service(
      IOIteratorNext(iterator.get()));
  if (unexpected_service) {
    LOG(ERROR) << "IOIteratorNext: too many services";
    return DiskImageStatusFailure;
  }

  return MediaResidesOnDiskImage(media, /*image_path=*/nullptr)
             ? DiskImageStatusTrue
             : DiskImageStatusFalse;
}

// Shows a dialog asking the user whether or not to install from the disk
// image.  Returns true if the user approves installation.
bool ShouldInstallDialog() {
  NSString* title = l10n_util::GetNSStringFWithFixup(
      IDS_INSTALL_FROM_DMG_TITLE, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  NSString* prompt = l10n_util::GetNSStringFWithFixup(
      IDS_INSTALL_FROM_DMG_PROMPT, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  NSString* yes = l10n_util::GetNSStringWithFixup(IDS_INSTALL_FROM_DMG_YES);
  NSString* no = l10n_util::GetNSStringWithFixup(IDS_INSTALL_FROM_DMG_NO);

  NSAlert* alert = [[NSAlert alloc] init];

  alert.alertStyle = NSAlertStyleInformational;
  alert.messageText = title;
  alert.informativeText = prompt;

  [alert addButtonWithTitle:yes];
  NSButton* cancel_button = [alert addButtonWithTitle:no];
  cancel_button.keyEquivalent = @"\e";

  NSInteger result = [alert runModal];

  return result == NSAlertFirstButtonReturn;
}

// Potentially shows an authorization dialog to request authentication to copy.
// If application_directory appears to be unwritable, attempts to obtain
// authorization, which may result in the display of the dialog. Returns null if
// authorization is not performed because it does not appear to be necessary
// because the user has permission to write to application_directory. Returns
// null if authorization fails.
base::mac::ScopedAuthorizationRef MaybeShowAuthorizationDialog(
    NSString* application_directory) {
  if ([NSFileManager.defaultManager
          isWritableFileAtPath:application_directory]) {
    return base::mac::ScopedAuthorizationRef();
  }

  NSString* prompt = l10n_util::GetNSStringFWithFixup(
      IDS_INSTALL_FROM_DMG_AUTHENTICATION_PROMPT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  return base::mac::AuthorizationCreateToRunAsRoot(
      base::apple::NSToCFPtrCast(prompt));
}

// Invokes the installer program at `installer_path` to copy `source_path` to
// `target_path` and perform any additional on-disk bookkeeping needed to be
// able to launch `target_path` properly. If `authorization_arg` is non-null,
// this function will invoke the installer with that authorization reference,
// and will attempt Keystone ticket promotion.
bool InstallFromDiskImage(base::mac::ScopedAuthorizationRef authorization,
                          NSURL* installer_url,
                          NSString* source_path,
                          NSString* target_path) {
  int exit_status;
  if (authorization) {
    const char* installer_path_c = installer_url.fileSystemRepresentation;
    const char* source_path_c = source_path.fileSystemRepresentation;
    const char* target_path_c = target_path.fileSystemRepresentation;
    const char* arguments[] = {source_path_c, target_path_c, nullptr};

    OSStatus status = base::mac::ExecuteWithPrivilegesAndWait(
        authorization, installer_path_c, kAuthorizationFlagDefaults, arguments,
        /*pipe=*/nullptr, &exit_status);
    if (status != errAuthorizationSuccess) {
      OSSTATUS_LOG(ERROR, status)
          << "AuthorizationExecuteWithPrivileges install";
      return false;
    }
  } else {
    NSError* error = nil;
    NSTask* task =
        [NSTask launchedTaskWithExecutableURL:installer_url
                                    arguments:@[ source_path, target_path ]
                                        error:&error
                           terminationHandler:nil];

    if (!task) {
      LOG(ERROR) << "NSTask launch error: "
                 << base::SysNSStringToUTF8(error.description);
      return false;
    }

    [task waitUntilExit];
    exit_status = task.terminationStatus;
  }

  if (exit_status != 0) {
    LOG(ERROR) << "install.sh: exit status " << exit_status;
    return false;
  }

  return true;
}

// Launches the application at `installed_path`. The helper application
// contained within `installed_path` will be used for the relauncher process.
// This keeps Launch Services from ever having to see or think about the helper
// application on the disk image. The relauncher process will be asked to call
// EjectAndTrashDiskImage on `dmg_bsd_device_name`.
bool LaunchInstalledApp(NSString* installed_path,
                        const std::string& dmg_bsd_device_name) {
  base::FilePath browser = base::apple::NSStringToFilePath(installed_path);

  base::FilePath helper = browser.Append("Contents/Frameworks");
  helper = helper.Append(chrome::kFrameworkName);
  helper = helper.Append("Versions");
  helper = helper.Append(chrome::kChromeVersion);
  helper = helper.Append("Helpers");
  helper = helper.Append(chrome::kHelperProcessExecutablePath);

  std::vector<std::string> args =
      base::CommandLine::ForCurrentProcess()->argv();

  std::vector<std::string> relauncher_args;
  if (!dmg_bsd_device_name.empty()) {
    std::string dmg_arg =
        base::StringPrintf("--%s=%s",
                           switches::kRelauncherProcessDMGDevice,
                           dmg_bsd_device_name.c_str());
    relauncher_args.push_back(dmg_arg);
  }

  return mac_relauncher::RelaunchAppAtPathWithHelper(helper, browser,
                                                     relauncher_args, args);
}

void ShowErrorDialog() {
  NSString* title = l10n_util::GetNSStringWithFixup(
      IDS_INSTALL_FROM_DMG_ERROR_TITLE);
  NSString* error = l10n_util::GetNSStringFWithFixup(
      IDS_INSTALL_FROM_DMG_ERROR, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  NSString* ok = l10n_util::GetNSStringWithFixup(IDS_OK);

  NSAlert* alert = [[NSAlert alloc] init];

  alert.alertStyle = NSAlertStyleWarning;
  alert.messageText = title;
  alert.informativeText = error;

  [alert addButtonWithTitle:ok];

  [alert runModal];
}

}  // namespace

DiskImageStatus IsAppRunningFromReadOnlyDiskImage(
    std::string* dmg_bsd_device_name) {
  return IsPathOnReadOnlyDiskImage(
      base::apple::OuterBundle().bundlePath.fileSystemRepresentation,
      dmg_bsd_device_name);
}

bool MaybeInstallFromDiskImage() {
  @autoreleasepool {
    std::string dmg_bsd_device_name;
    if (IsAppRunningFromReadOnlyDiskImage(&dmg_bsd_device_name) !=
        DiskImageStatusTrue) {
      return false;
    }

    NSArray* application_directories = NSSearchPathForDirectoriesInDomains(
        NSApplicationDirectory, NSLocalDomainMask, YES);
    if (!application_directories.count) {
      LOG(ERROR) << "NSSearchPathForDirectoriesInDomains: "
                 << "no local application directories";
      return false;
    }
    NSString* application_directory = application_directories.firstObject;

    NSFileManager* file_manager = NSFileManager.defaultManager;

    BOOL is_directory;
    if (![file_manager fileExistsAtPath:application_directory
                            isDirectory:&is_directory] ||
        !is_directory) {
      VLOG(1) << "No application directory at "
              << base::SysNSStringToUTF8(application_directory);
      return false;
    }

    NSString* source_path = base::apple::OuterBundle().bundlePath;
    NSString* application_name = source_path.lastPathComponent;
    NSString* target_path =
        [application_directory stringByAppendingPathComponent:application_name];

    if ([file_manager fileExistsAtPath:target_path]) {
      VLOG(1) << "Something already exists at "
              << base::SysNSStringToUTF8(target_path);
      return false;
    }

    NSURL* installer_url =
        [base::apple::FrameworkBundle() URLForResource:@"install"
                                         withExtension:@"sh"];
    if (!installer_url) {
      VLOG(1) << "Could not locate install.sh";
      return false;
    }

    if (!ShouldInstallDialog()) {
      return false;
    }

    base::mac::ScopedAuthorizationRef authorization =
        MaybeShowAuthorizationDialog(application_directory);
    // `authorization` will be null if it's deemed unnecessary or if
    // authentication fails.  In either case, try to install without privilege
    // escalation.

    if (!InstallFromDiskImage(std::move(authorization), installer_url,
                              source_path, target_path)) {
      ShowErrorDialog();
      return false;
    }

    dock::AddIcon(target_path, source_path);

    if (dmg_bsd_device_name.empty()) {
      // Not fatal, just diagnostic.
      LOG(ERROR) << "Could not determine disk image BSD device name";
    }

    if (!LaunchInstalledApp(target_path, dmg_bsd_device_name)) {
      ShowErrorDialog();
      return false;
    }

    return true;
  }
}

namespace {

// A simple scoper that calls DASessionScheduleWithRunLoop when created and
// DASessionUnscheduleFromRunLoop when destroyed.
class ScopedDASessionScheduleWithRunLoop {
 public:
  ScopedDASessionScheduleWithRunLoop(DASessionRef session,
                                     CFRunLoopRef run_loop,
                                     CFStringRef run_loop_mode)
      : session_(session),
        run_loop_(run_loop),
        run_loop_mode_(run_loop_mode) {
    DASessionScheduleWithRunLoop(session_, run_loop_, run_loop_mode_);
  }

  ScopedDASessionScheduleWithRunLoop(
      const ScopedDASessionScheduleWithRunLoop&) = delete;
  ScopedDASessionScheduleWithRunLoop& operator=(
      const ScopedDASessionScheduleWithRunLoop&) = delete;

  ~ScopedDASessionScheduleWithRunLoop() {
    DASessionUnscheduleFromRunLoop(session_, run_loop_, run_loop_mode_);
  }

 private:
  DASessionRef session_;
  CFRunLoopRef run_loop_;
  CFStringRef run_loop_mode_;
};

// A small structure used to ferry data between SynchronousDAOperation and
// SynchronousDACallbackAdapter.
struct SynchronousDACallbackData {
  base::apple::ScopedCFTypeRef<DADissenterRef> dissenter;
  bool callback_called = false;
  bool run_loop_running = false;
  bool can_log = true;
};

// The callback target for SynchronousDAOperation. Set the fields in
// SynchronousDACallbackData properly and then stops the run loop so that
// SynchronousDAOperation may proceed.
void SynchronousDACallbackAdapter(DADiskRef disk,
                                  DADissenterRef dissenter,
                                  void* context) {
  SynchronousDACallbackData* callback_data =
      static_cast<SynchronousDACallbackData*>(context);
  callback_data->callback_called = true;

  if (dissenter) {
    callback_data->dissenter.reset(dissenter, base::scoped_policy::RETAIN);
  }

  // Only stop the run loop if SynchronousDAOperation started it. Don't stop
  // anything if this callback was reached synchronously from DADiskUnmount or
  // DADiskEject.
  if (callback_data->run_loop_running) {
    CFRunLoopStop(CFRunLoopGetCurrent());
  }
}

// Performs a DiskArbitration operation synchronously. After the operation is
// requested by SynchronousDADiskUnmount or SynchronousDADiskEject, those
// functions will call this one to run a run loop for a period of time,
// waiting for the callback to be called. When the callback is called, the
// run loop will be stopped, and this function will examine the result. If
// a dissenter prevented the operation from completing, or if the run loop
// timed out without the callback being called, this function will return
// false. When the callback completes successfully with no dissenters within
// the time allotted, this function returns true. This function requires that
// the DASession being used for the operation being performed has been added
// to the current run loop with DASessionScheduleWithRunLoop.
bool SynchronousDAOperation(const char* name,
                            SynchronousDACallbackData* callback_data) {
  // The callback may already have been called synchronously. In that case,
  // avoid spinning the run loop at all.
  if (!callback_data->callback_called) {
    const CFTimeInterval kOperationTimeoutSeconds = 15;
    base::AutoReset<bool> running_reset(&callback_data->run_loop_running, true);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, kOperationTimeoutSeconds, FALSE);
  }

  if (!callback_data->callback_called) {
    LOG_IF(ERROR, callback_data->can_log) << name << ": timed out";
    return false;
  } else if (callback_data->dissenter) {
    if (callback_data->can_log) {
      CFStringRef status_string_cf =
          DADissenterGetStatusString(callback_data->dissenter.get());
      std::string status_string;
      if (status_string_cf) {
        status_string.assign(" ");
        status_string.append(base::SysCFStringRefToUTF8(status_string_cf));
      }
      LOG(ERROR) << name << ": dissenter: "
                 << DADissenterGetStatus(callback_data->dissenter.get())
                 << status_string;
    }
    return false;
  }

  return true;
}

// Calls DADiskUnmount synchronously, returning the result.
bool SynchronousDADiskUnmount(DADiskRef disk,
                              DADiskUnmountOptions options,
                              bool can_log) {
  SynchronousDACallbackData callback_data;
  callback_data.can_log = can_log;
  DADiskUnmount(disk, options, SynchronousDACallbackAdapter, &callback_data);
  return SynchronousDAOperation("DADiskUnmount", &callback_data);
}

// Calls DADiskEject synchronously, returning the result.
bool SynchronousDADiskEject(DADiskRef disk, DADiskEjectOptions options) {
  SynchronousDACallbackData callback_data;
  DADiskEject(disk, options, SynchronousDACallbackAdapter, &callback_data);
  return SynchronousDAOperation("DADiskEject", &callback_data);
}

}  // namespace

void EjectAndTrashDiskImage(const std::string& dmg_bsd_device_name) {
  base::apple::ScopedCFTypeRef<DASessionRef> session(
      DASessionCreate(/*allocator=*/nullptr));
  if (!session.get()) {
    LOG(ERROR) << "DASessionCreate";
    return;
  }

  base::apple::ScopedCFTypeRef<DADiskRef> disk(DADiskCreateFromBSDName(
      /*allocator=*/nullptr, session.get(), dmg_bsd_device_name.c_str()));
  if (!disk.get()) {
    LOG(ERROR) << "DADiskCreateFromBSDName";
    return;
  }

  // dmg_bsd_device_name may only refer to part of the disk: it may be a
  // single filesystem on a larger disk. Use the "whole disk" object to
  // be able to unmount all mounted filesystems from the disk image, and eject
  // the image. This is harmless if dmg_bsd_device_name already referred to a
  // "whole disk."
  disk.reset(DADiskCopyWholeDisk(disk.get()));
  if (!disk.get()) {
    LOG(ERROR) << "DADiskCopyWholeDisk";
    return;
  }

  base::mac::ScopedIOObject<io_service_t> media(DADiskCopyIOMedia(disk.get()));
  if (!media.get()) {
    LOG(ERROR) << "DADiskCopyIOMedia";
    return;
  }

  // Make sure the device is a disk image, and get the path to its disk image
  // file.
  std::string disk_image_path;
  if (!MediaResidesOnDiskImage(media, &disk_image_path)) {
    LOG(ERROR) << "MediaResidesOnDiskImage";
    return;
  }

  // SynchronousDADiskUnmount and SynchronousDADiskEject require that the
  // session be scheduled with the current run loop.
  ScopedDASessionScheduleWithRunLoop session_run_loop(
      session.get(), CFRunLoopGetCurrent(), kCFRunLoopCommonModes);

  // Retry the unmount in a loop to give anything that may have been in use on
  // the disk image (such as crashpad_handler) a chance to exit.
  int tries = 15;
  while (!SynchronousDADiskUnmount(disk.get(), kDADiskUnmountOptionWhole,
                                   --tries == 0)) {
    if (tries == 0) {
      LOG(ERROR) << "SynchronousDADiskUnmount";
      return;
    }
    sleep(1);
  }

  if (!SynchronousDADiskEject(disk.get(), kDADiskEjectOptionDefault)) {
    LOG(ERROR) << "SynchronousDADiskEject";
    return;
  }

  NSURL* disk_image_path_nsurl =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(disk_image_path)];
  NSError* error = nil;
  if (![NSFileManager.defaultManager trashItemAtURL:disk_image_path_nsurl
                                   resultingItemURL:nil
                                              error:&error]) {
    LOG(ERROR) << base::SysNSStringToUTF8(error.localizedDescription);
    return;
  }
}
