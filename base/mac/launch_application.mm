// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/launch_application.h"

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/types/expected.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace base::mac {

namespace {

NSArray* CommandLineArgsToArgsArray(const CommandLineArgs& command_line_args) {
  if (const CommandLine* command_line =
          absl::get_if<CommandLine>(&command_line_args)) {
    const auto& argv = command_line->argv();
    size_t argc = argv.size();
    DCHECK_GT(argc, 0lu);

    NSMutableArray* args_array = [NSMutableArray arrayWithCapacity:argc - 1];
    // NSWorkspace automatically adds the binary path as the first argument and
    // thus it should not be included in the list.
    for (size_t i = 1; i < argc; ++i) {
      [args_array addObject:base::SysUTF8ToNSString(argv[i])];
    }

    return args_array;
  }

  if (const std::vector<std::string>* string_vector =
          absl::get_if<std::vector<std::string>>(&command_line_args)) {
    NSMutableArray* args_array =
        [NSMutableArray arrayWithCapacity:string_vector->size()];
    for (const auto& arg : *string_vector) {
      [args_array addObject:base::SysUTF8ToNSString(arg)];
    }
  }

  return @[];
}

NSWorkspaceOpenConfiguration* GetOpenConfiguration(
    LaunchApplicationOptions options,
    const CommandLineArgs& command_line_args) API_AVAILABLE(macos(10.15)) {
  NSWorkspaceOpenConfiguration* config =
      [NSWorkspaceOpenConfiguration configuration];

  config.activates = options.activate;
  config.createsNewApplicationInstance = options.create_new_instance;
  config.promptsUserIfNeeded = options.prompt_user_if_needed;
  config.arguments = CommandLineArgsToArgsArray(command_line_args);

  return config;
}

NSWorkspaceLaunchOptions GetLaunchOptions(LaunchApplicationOptions options) {
  NSWorkspaceLaunchOptions launch_options = NSWorkspaceLaunchDefault;

  if (!options.activate) {
    launch_options |= NSWorkspaceLaunchWithoutActivation;
  }
  if (options.create_new_instance) {
    launch_options |= NSWorkspaceLaunchNewInstance;
  }
  if (options.prompt_user_if_needed) {
    launch_options |= NSWorkspaceLaunchWithErrorPresentation;
  }

  return launch_options;
}

}  // namespace

void LaunchApplication(const base::FilePath& app_bundle_path,
                       const CommandLineArgs& command_line_args,
                       const std::vector<std::string>& url_specs,
                       LaunchApplicationOptions options,
                       LaunchApplicationCallback callback) {
  __block LaunchApplicationCallback callback_block_access = std::move(callback);

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
              std::move(callback_block_access).Run(nil, error);
            } else {
              std::move(callback_block_access).Run(app, nil);
            }
          });
        };

    NSWorkspaceOpenConfiguration* configuration =
        GetOpenConfiguration(options, command_line_args);

    if (ns_urls) {
      [NSWorkspace.sharedWorkspace openURLs:ns_urls
                       withApplicationAtURL:bundle_url
                              configuration:configuration
                          completionHandler:action_block];
    } else {
      [NSWorkspace.sharedWorkspace openApplicationAtURL:bundle_url
                                          configuration:configuration
                                      completionHandler:action_block];
    }
  } else {
    NSDictionary* configuration = @{
      NSWorkspaceLaunchConfigurationArguments :
          CommandLineArgsToArgsArray(command_line_args),
    };

    NSWorkspaceLaunchOptions launch_options = GetLaunchOptions(options);

    NSError* error = nil;
    NSRunningApplication* app;
    if (ns_urls) {
      app = [NSWorkspace.sharedWorkspace openURLs:ns_urls
                             withApplicationAtURL:bundle_url
                                          options:launch_options
                                    configuration:configuration
                                            error:&error];
    } else {
      app = [NSWorkspace.sharedWorkspace launchApplicationAtURL:bundle_url
                                                        options:launch_options
                                                  configuration:configuration
                                                          error:&error];
    }

    dispatch_async(dispatch_get_main_queue(), ^{
      if (error) {
        LOG(ERROR) << base::SysNSStringToUTF8(error.localizedDescription);
        std::move(callback_block_access).Run(nil, error);
      } else {
        std::move(callback_block_access).Run(app, nil);
      }
    });
  }
}

}  // namespace base::mac
