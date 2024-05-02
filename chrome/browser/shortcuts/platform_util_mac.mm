// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/platform_util_mac.h"

#import <AppKit/AppKit.h>

#import "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/safe_base_name.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"

namespace shortcuts {

namespace {
base::LazyThreadPoolSequencedTaskRunner g_set_icon_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits({base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                          base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
}  // namespace

void SetIconForFile(NSImage* image,
                    const base::FilePath& file,
                    base::OnceCallback<void(bool)> callback) {
  g_set_icon_task_runner.Get()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](NSImage* image, const base::FilePath& file) {
            return [NSWorkspace.sharedWorkspace
                setIcon:image
                forFile:base::apple::FilePathToNSString(file)
                options:NSExcludeQuickDrawElementsIconCreationOption];
          },
          image, file),
      std::move(callback));
}

void SetDefaultApplicationToOpenFile(
    NSURL* file_url,
    NSURL* application_url,
    base::OnceCallback<void(NSError*)> callback) {
  if (@available(macOS 12.0, *)) {
    [NSWorkspace.sharedWorkspace
        setDefaultApplicationAtURL:application_url
                   toOpenFileAtURL:file_url
                 completionHandler:base::CallbackToBlock(
                                       base::BindPostTaskToCurrentDefault(
                                           std::move(callback)))];
  } else {
    // Older macOS versions don't have a nice API for this, but doing what
    // setDefaultApplicationAtURL:toOpenFileAtURL: does directly seems to work
    // just fine on those versions as well, so that is what this branch does.
    NSError* error = nil;
    [file_url setResourceValue:application_url
                        forKey:@"_NSURLStrongBindingKey"
                         error:&error];
    base::BindPostTaskToCurrentDefault(std::move(callback)).Run(error);
  }
}

std::optional<base::SafeBaseName> SanitizeTitleForFileName(
    const std::string& title) {
  // Strip all preceding '.'s from the path.
  std::string::size_type first_non_dot = title.find_first_not_of(".");
  if (first_non_dot == std::string::npos) {
    return std::nullopt;
  }
  std::string name = title.substr(first_non_dot);

  // Finder will display ':' as '/', so replace all '/' instances with ':'.
  base::ranges::replace(name, '/', ':');

  return base::SafeBaseName::Create(name);
}

}  // namespace shortcuts
