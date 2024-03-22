// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test_utils_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
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

void SetUpAuthForTesting() {
  Java_SyncTestSigninUtils_setUpAuthForTesting(
      base::android::AttachCurrentThread());
}

void TearDownAuthForTesting() {
  base::RunLoop run_loop;
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::BindLambdaForTesting([&]() {
                               Java_SyncTestSigninUtils_tearDownAuthForTesting(
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

}  // namespace sync_test_utils_android
