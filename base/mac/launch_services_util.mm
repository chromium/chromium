// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/launch_services_util.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"

namespace base::mac {

namespace {

NSArray* CommandLineToArgsArray(const CommandLine& command_line) {
  const auto& argv = command_line.argv();
  size_t argc = argv.size();
  NSMutableArray* args_array = [NSMutableArray arrayWithCapacity:argc - 1];
  // NSWorkspace automatically adds the binary path as the first argument and
  // thus it should not be included in the list.
  for (size_t i = 1; i < argc; ++i) {
    [args_array addObject:base::SysUTF8ToNSString(argv[i])];
  }

  return args_array;
}

NSWorkspaceOpenConfiguration* GetOpenConfiguration(
    OpenApplicationOptions options,
    const CommandLine& command_line) API_AVAILABLE(macos(10.15)) {
  NSWorkspaceOpenConfiguration* config =
      [NSWorkspaceOpenConfiguration configuration];

  config.activates = options.activate;
  config.createsNewApplicationInstance = options.create_new_instance;

  config.arguments = CommandLineToArgsArray(command_line);

  return config;
}

NSWorkspaceLaunchOptions GetLaunchOptions(OpenApplicationOptions options) {
  NSWorkspaceLaunchOptions launch_options = NSWorkspaceLaunchDefault;

  if (!options.activate) {
    launch_options |= NSWorkspaceLaunchWithoutActivation;
  }
  if (options.create_new_instance) {
    launch_options |= NSWorkspaceLaunchNewInstance;
  }

  return launch_options;
}

}  // namespace

void OpenApplication(const base::FilePath& app_bundle_path,
                     const CommandLine& command_line,
                     const std::vector<std::string>& url_specs,
                     OpenApplicationOptions options,
                     ApplicationOpenedCallback callback) {
  __block ApplicationOpenedCallback callback_block_access = std::move(callback);

  NSURL* bundle_url = FilePathToNSURL(app_bundle_path);
  if (!bundle_url) {
    dispatch_async(dispatch_get_main_queue(), ^{
      std::move(callback_block_access)
          .Run(nil, [NSError errorWithDomain:NSCocoaErrorDomain
                                        code:NSFileNoSuchFileError
                                    userInfo:nil]);
    });
    return;
  }

  NSMutableArray* ns_urls = nil;
  if (!url_specs.empty()) {
    ns_urls = [NSMutableArray arrayWithCapacity:url_specs.size()];
    for (const auto& url_spec : url_specs) {
      [ns_urls
          addObject:[NSURL URLWithString:base::SysUTF8ToNSString(url_spec)]];
    }
  }

  if (@available(macOS 10.15, *)) {
    void (^action_block)(NSRunningApplication*, NSError*) =
        ^void(NSRunningApplication* app, NSError* error) {
          dispatch_async(dispatch_get_main_queue(), ^{
            if (error) {
              LOG(ERROR) << base::SysNSStringToUTF8(error.localizedDescription);
            }
            std::move(callback_block_access).Run(app, error);
          });
        };

    if (ns_urls) {
      [[NSWorkspace sharedWorkspace]
                      openURLs:ns_urls
          withApplicationAtURL:bundle_url
                 configuration:GetOpenConfiguration(options, command_line)
             completionHandler:action_block];
    } else {
      [[NSWorkspace sharedWorkspace]
          openApplicationAtURL:bundle_url
                 configuration:GetOpenConfiguration(options, command_line)
             completionHandler:action_block];
    }
  } else {
    NSDictionary* configuration = @{
      NSWorkspaceLaunchConfigurationArguments :
          CommandLineToArgsArray(command_line),
    };

    NSError* error;
    NSRunningApplication* app;
    if (ns_urls) {
      app = [[NSWorkspace sharedWorkspace] openURLs:ns_urls
                               withApplicationAtURL:bundle_url
                                            options:GetLaunchOptions(options)
                                      configuration:configuration
                                              error:&error];
    } else {
      app = [[NSWorkspace sharedWorkspace]
          launchApplicationAtURL:bundle_url
                         options:GetLaunchOptions(options)
                   configuration:configuration
                           error:&error];
    }

    if (!app) {
      LOG(ERROR) << base::SysNSStringToUTF8(error.localizedDescription);
    }

    dispatch_async(dispatch_get_main_queue(), ^{
      std::move(callback_block_access).Run(app, error);
    });
  }
}

}  // namespace base::mac
