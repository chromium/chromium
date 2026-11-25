// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <jni.h>

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/platform_util_internal.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/util/jni_headers/PlatformUtil_jni.h"

namespace platform_util {
namespace {

// DocumentsUI has no item-selection intent, so OpenItem(..., OPEN_FOLDER) and
// ShowItemInFolder() share the same ACTION_VIEW + MIME_TYPE_DIR intent on
// `path` directly (no DirName()): directory URIs open that folder, while
// DocumentsUI pops file URIs off its stack to open their parent folder.
void ShowContentUriInFolder(const base::FilePath& path) {
  std::optional<base::FilePath> content_uri = base::ResolveToContentUri(path);
  if (!content_uri) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  PlatformUtilJni::showItemInFolder(env, content_uri->value());
}

}  // namespace

namespace internal {

void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type) {
  switch (type) {
    case OPEN_FILE:
      // Note: platform_util.cc still invokes OpenItem()'s callback with
      // OPEN_SUCCEEDED after this returns.
      NOTIMPLEMENTED();
      return;
    case OPEN_FOLDER:
      ShowContentUriInFolder(path);
      return;
  }
}

}  // namespace internal

// TODO: crbug.com/40736101 to track implementation of the following methods.

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  // Skip opening the folder in some browser tests.
  if (!internal::AreShellOperationsAllowed()) {
    return;
  }
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ShowContentUriInFolder, full_path));
}

void OpenExternal(const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  PlatformUtilJni::launchExternalProtocol(env, url.spec());
}

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  NOTIMPLEMENTED();
  return view->GetWindowAndroid();
}

gfx::NativeView GetParent(gfx::NativeView view) {
  NOTIMPLEMENTED();
  return view;
}

bool IsWindowActive(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

void ActivateWindow(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
}

bool IsVisible(gfx::NativeView view) {
  NOTIMPLEMENTED();
  return true;
}

}  // namespace platform_util

DEFINE_JNI(PlatformUtil)
