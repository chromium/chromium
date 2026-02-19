// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/initialize_feature_list_android.h"

#include "base/allocator/partition_alloc_support.h"
#include "base/profiler/thread_group_profiler.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/default_clock.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/profiler/chrome_thread_group_profiler_client.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/startup_helper.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/base_module_jni/InitializeFeatureList_jni.h"

namespace {

bool did_init_feature_list_early = false;

}  // namespace

namespace variations::android {

static void JNI_InitializeFeatureList_InitializeFeatureList(JNIEnv* env) {
  chrome::RegisterPathProvider();
  // The ThreadGroupProfiler client must be set before thread pool is created.
  base::ThreadGroupProfiler::SetClient(
      std::make_unique<ChromeThreadGroupProfilerClient>());
  base::ThreadPoolInstance::Create("Browser");
  // No specified process type means this is the Browser process.
  base::allocator::PartitionAllocSupport::Get()->ReconfigureEarlyish("");
  // Register the TaskExecutor for posting task to the BrowserThreads. It is
  // incorrect to post to a BrowserThread before this point. This instantiates
  // and binds the MessageLoopForUI on the main thread (but it's only labeled
  // as BrowserThread::UI in BrowserMainLoop::CreateMainMessageLoop).
  content::CreateBrowserTaskExecutor();
  variations::VariationsIdsProvider::CreateInstance(
      variations::VariationsIdsProvider::Mode::kUseSignedInState,
      std::make_unique<base::DefaultClock>());

  ChromeFeatureListCreator* chrome_feature_list_creator =
      ChromeFeatureListCreator::GetInstance();
  chrome_feature_list_creator->CreateFeatureList();

  // The FeatureList needs to be created before starting the ThreadPool.
  content::StartThreadPool();
  content::InstallPartitionAllocSchedulerLoopQuarantineTaskObserver();

  // No specified process type means this is the Browser process.
  base::allocator::PartitionAllocSupport::Get()
      ->ReconfigureAfterFeatureListInit("");
  base::allocator::PartitionAllocSupport::Get()->ReconfigureAfterTaskRunnerInit(
      "");

  did_init_feature_list_early = true;
}

bool DidInitFeatureListEarly() {
  return did_init_feature_list_early;
}

}  // namespace variations::android

DEFINE_JNI(InitializeFeatureList)
