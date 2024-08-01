// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/android/content/web_contents_factory_data_deleter.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/android/content/jni_headers/WebContentsFactory_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jobject> JNI_WebContentsFactory_CreateWebContents(
    JNIEnv* env,
    Profile* profile,
    jboolean initially_hidden,
    jboolean initialize_renderer,
    jlong j_target_network,
    const JavaParamRef<jthrowable>& j_creator_location) {
  content::WebContents::CreateParams params(profile);
  params.initially_hidden = static_cast<bool>(initially_hidden);
  params.desired_renderer_state =
      static_cast<bool>(initialize_renderer)
          ? content::WebContents::CreateParams::
                kInitializeAndWarmupRendererProcess
          : content::WebContents::CreateParams::kOkayToHaveRendererProcess;
  params.target_network = j_target_network;
  params.java_creator_location = j_creator_location;

  // Ownership is passed into java, and then to TabAndroid::InitWebContents.
  return content::WebContents::Create(params).release()->GetJavaWebContents();
}

static ScopedJavaLocalRef<jobject>
JNI_WebContentsFactory_CreateWebContentsWithSeparateStoragePartitionForExperiment(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jthrowable>& j_creator_location) {
  auto partition_config = content::StoragePartitionConfig::Create(
      profile,
      /*partition_domain=*/"MayLaunchUrlUsesSeparateStoragePartitionDomain",
      /*partition_name=*/"1", /*in_memory=*/true);

  auto site_instance = content::SiteInstance::CreateForFixedStoragePartition(
      profile, GURL(), partition_config);

  content::WebContents::CreateParams params =
      content::WebContents::CreateParams(profile, std::move(site_instance));

  params.initially_hidden = true;
  // Equivalent to `initialize_renderer` == true in
  // `JNI_WebContentsFactory_CreateWebContents`.
  params.desired_renderer_state =
      content::WebContents::CreateParams::kInitializeAndWarmupRendererProcess;
  params.java_creator_location = j_creator_location;

  // Ownership is passed into java, and then to TabAndroid::InitWebContents.
  auto web_contents = content::WebContents::Create(params);

  // StoragePartitions don't usually get deleted until the
  // BrowserContext/Profile is shutdown but we want to tie this one's to the
  // lifetime of the WebContents. To do so we'll use an observer to manually
  // delete all the partition's data which has the same effect.

  // WebContentsFactoryDataDeleter owns itself and is also bound to
  // `web_contents` lifetime by observing WebContentsDestroyed().
  new WebContentsFactoryDataDeleter(
      web_contents.get(),
      web_contents->GetSiteInstance()->GetStoragePartitionConfig());

  return web_contents.release()->GetJavaWebContents();
}
