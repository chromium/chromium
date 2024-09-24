// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test_utils_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/test/sync_integration_test_support_jni_headers/SyncTestSigninUtils_jni.h"

namespace sync_test_utils_android {

void SetUpAccountAndSignInForTesting() {
  base::RunLoop run_loop;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()}, base::BindLambdaForTesting([&]() {
        Java_SyncTestSigninUtils_setUpAccountAndSignInForTesting(
            base::android::AttachCurrentThread());
        run_loop.Quit();
      }));
  run_loop.Run();
}

void SetUpAccountAndSignInAndEnableSyncForTesting() {
  base::RunLoop run_loop;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()}, base::BindLambdaForTesting([&]() {
        Java_SyncTestSigninUtils_setUpAccountAndSignInAndEnableSyncForTesting(
            base::android::AttachCurrentThread());
        run_loop.Quit();
      }));
  run_loop.Run();
}

void SignOutForTesting() {
  base::RunLoop run_loop;
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::BindLambdaForTesting([&]() {
                               Java_SyncTestSigninUtils_signOutForTesting(
                                   base::android::AttachCurrentThread());
                               run_loop.Quit();
                             }));
  run_loop.Run();
}

void SetUpFakeAuthForTesting() {
  Java_SyncTestSigninUtils_setUpFakeAuthForTesting(
      base::android::AttachCurrentThread());
}

void TearDownFakeAuthForTesting() {
  base::RunLoop run_loop;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()}, base::BindLambdaForTesting([&]() {
        Java_SyncTestSigninUtils_tearDownFakeAuthForTesting(
            base::android::AttachCurrentThread());
        run_loop.Quit();
      }));
  run_loop.Run();
}

void SetUpLiveAccountAndSignInForTesting(const std::string& username,
                                         const std::string& password) {
  base::RunLoop run_loop;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()}, base::BindLambdaForTesting([&]() {
        JNIEnv* env = base::android::AttachCurrentThread();
        Java_SyncTestSigninUtils_setUpLiveAccountAndSignInForTesting(
            env, base::android::ConvertUTF8ToJavaString(env, username),
            base::android::ConvertUTF8ToJavaString(env, password));
        run_loop.Quit();
      }));
  run_loop.Run();
}

void SetUpLiveAccountAndSignInAndEnableSyncForTesting(
    const std::string& username,
    const std::string& password) {
  base::RunLoop run_loop;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()}, base::BindLambdaForTesting([&]() {
        JNIEnv* env = base::android::AttachCurrentThread();
        Java_SyncTestSigninUtils_setUpLiveAccountAndSignInAndEnableSyncForTesting(
            env, base::android::ConvertUTF8ToJavaString(env, username),
            base::android::ConvertUTF8ToJavaString(env, password));
        run_loop.Quit();
      }));
  run_loop.Run();
}

void ShutdownLiveAuthForTesting() {
  base::RunLoop run_loop;
  // The heap instance of the callback will be deleted by
  // JNI_SyncTestSigninUtils_OnShutdownComplete when shutdown is completed.
  auto heap_callback =
      std::make_unique<base::OnceClosure>(run_loop.QuitClosure());

  Java_SyncTestSigninUtils_shutdownLiveAuthForTesting(
      base::android::AttachCurrentThread(),
      reinterpret_cast<intptr_t>(heap_callback.release()));

  run_loop.Run();
}

void JNI_SyncTestSigninUtils_OnShutdownComplete(JNIEnv* env,
                                                jlong callbackPtr) {
  std::unique_ptr<base::OnceClosure> heap_callback(
      reinterpret_cast<base::OnceClosure*>(callbackPtr));
  std::move(*heap_callback).Run();
}

}  // namespace sync_test_utils_android
