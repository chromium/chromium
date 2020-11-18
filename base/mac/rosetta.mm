// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/rosetta.h"

#include <AppKit/AppKit.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <mach/machine.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

namespace base {
namespace mac {

#if defined(ARCH_CPU_X86_64)

// https://developer.apple.com/documentation/apple_silicon/about_the_rosetta_translation_environment#3616845
bool ProcessIsTranslated() {
  int ret = 0;
  size_t size = sizeof(ret);
  if (sysctlbyname("sysctl.proc_translated", &ret, &size, nullptr, 0) == -1)
    return false;
  return ret;
}

#endif  // ARCH_CPU_X86_64

#if defined(ARCH_CPU_ARM64)

bool IsRosettaInstalled() {
  // Chromium currently requires the 10.15 SDK, but code compiled for Arm must
  // be compiled against at least the 11.0 SDK and will run on at least macOS
  // 11.0, so this is safe. __builtin_available doesn't work for 11.0 yet;
  // https://crbug.com/1115294
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  return CFBundleIsArchitectureLoadable(CPU_TYPE_X86_64);
#pragma clang diagnostic pop
}

void RequestRosettaInstallation(
    const string16& title_text,
    const string16& body_text,
    OnceCallback<void(RosettaInstallationResult)> callback) {
  DCHECK([NSThread isMainThread]);

  if (IsRosettaInstalled()) {
    std::move(callback).Run(RosettaInstallationResult::kAlreadyInstalled);
    return;
  }

  static NSObject* current_rosetta_installation = nil;
  if (current_rosetta_installation) {
    if ([current_rosetta_installation
            respondsToSelector:@selector(windowController)]) {
      NSWindowController* windowController = [current_rosetta_installation
          performSelector:@selector(windowController)];
      if (windowController &&
          [windowController isKindOfClass:[NSWindowController class]]) {
        [windowController showWindow:nil];
      }
    }
    return;
  }

  @autoreleasepool {
    static const NoDestructor<scoped_nsobject<NSBundle>> bundle([]() {
      scoped_nsobject<NSBundle> bundle(
          [[NSBundle alloc] initWithPath:@"/System/Library/PrivateFrameworks/"
                                         @"OAHSoftwareUpdate.framework"]);
      if (![bundle load])
        return scoped_nsobject<NSBundle>();

      return bundle;
    }());
    if (!bundle.get()) {
      std::move(callback).Run(RosettaInstallationResult::kFailedToAccessSPI);
      return;
    }

    // The method being called is:
    //
    // - (void)[OAHSoftwareUpdateController
    //      startUpdateWithOptions:(NSDictionary*)options
    //              withHostWindow:(NSWindow*)window
    //                  completion:(void (^)(BOOL))block]
    SEL selector = @selector(startUpdateWithOptions:withHostWindow:completion:);

    scoped_nsobject<NSObject> controller(
        [[NSClassFromString(@"OAHSoftwareUpdateController") alloc] init]);
    NSMethodSignature* signature =
        [controller methodSignatureForSelector:selector];
    if (!signature) {
      std::move(callback).Run(RosettaInstallationResult::kFailedToAccessSPI);
      return;
    }
    if (strcmp(signature.methodReturnType, "v") != 0 ||
        signature.numberOfArguments != 5 ||
        strcmp([signature getArgumentTypeAtIndex:0], "@") != 0 ||
        strcmp([signature getArgumentTypeAtIndex:1], ":") != 0 ||
        strcmp([signature getArgumentTypeAtIndex:2], "@") != 0 ||
        strcmp([signature getArgumentTypeAtIndex:3], "@") != 0 ||
        strcmp([signature getArgumentTypeAtIndex:4], "@?") != 0) {
      std::move(callback).Run(RosettaInstallationResult::kFailedToAccessSPI);
      return;
    }

    NSInvocation* invocation =
        [NSInvocation invocationWithMethodSignature:signature];

    current_rosetta_installation = [controller.get() retain];
    invocation.target = current_rosetta_installation;
    invocation.selector = selector;

    NSDictionary* options = @{
      @"TitleText" : SysUTF16ToNSString(title_text),
      @"BodyText" : SysUTF16ToNSString(body_text)
    };
    [invocation setArgument:&options atIndex:2];

    NSWindow* window = nil;
    [invocation setArgument:&window atIndex:3];

    __block OnceCallback<void(RosettaInstallationResult)> block_callback =
        std::move(callback);
    auto completion = ^(BOOL success) {
      // There _should_ be a valid callback and current_rosetta_installation
      // here. However, crashes indicate that sometimes
      // OAHSoftwareUpdateController performs a double-callback of the block.
      // Therefore, be paranoid.
      if (current_rosetta_installation) {
        [current_rosetta_installation release];
        current_rosetta_installation = nil;
      }
      if (block_callback) {
        std::move(block_callback)
            .Run(success ? RosettaInstallationResult::kInstallationSuccess
                         : RosettaInstallationResult::kInstallationFailure);
      }
    };

    [invocation setArgument:&completion atIndex:4];

    [invocation invoke];
  }
}

#endif  // ARCH_CPU_ARM64

bool RequestRosettaAheadOfTimeTranslation(
    const std::vector<FilePath>& binaries) {
#if defined(ARCH_CPU_X86_64)
  if (!ProcessIsTranslated())
    return false;
#endif  // ARCH_CPU_X86_64

  // It's unclear if this is a BOOL or Boolean or bool. It's returning 0 or 1
  // in w0, so just grab the result as an int and convert here.
  // int rosetta_translate_binaries(const char*[] paths, int npaths)
  using rosetta_translate_binaries_t = int (*)(const char*[], int);
  static auto rosetta_translate_binaries = []() {
    void* librosetta =
        dlopen("/usr/lib/libRosetta.dylib", RTLD_LAZY | RTLD_LOCAL);
    if (!librosetta)
      return static_cast<rosetta_translate_binaries_t>(nullptr);
    return reinterpret_cast<rosetta_translate_binaries_t>(
        dlsym(librosetta, "rosetta_translate_binaries"));
  }();
  if (!rosetta_translate_binaries)
    return false;

  std::vector<std::string> paths;
  std::vector<const char*> path_strs;
  paths.reserve(binaries.size());
  path_strs.reserve(paths.size());
  for (const auto& binary : binaries) {
    paths.push_back(FilePath::GetHFSDecomposedForm(binary.value()));
    path_strs.push_back(paths.back().c_str());
  }

  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  return rosetta_translate_binaries(path_strs.data(), path_strs.size());
}

}  // namespace mac
}  // namespace base
