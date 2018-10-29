// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/common/unindexed_ruleset.h"
#include "content/public/browser/browser_thread.h"
#include "jni/TestSubresourceFilterPublisher_jni.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

// TODO(csharrison): This whole file is a hack, because Android cannot use
// native files that are test-only. So, this is a duplication and simplification
// of a lot of subresource_filter test harness code. Because it is compiled into
// a normal build of Chrome, I've tried to strip it down as much as possible.
// Once native test files can be used on Android, most all of this can be
// deleted.

using content::BrowserThread;

namespace {

url_pattern_index::proto::UrlRule CreateSuffixRule(const std::string& suffix) {
  url_pattern_index::proto::UrlRule rule;
  rule.set_semantics(url_pattern_index::proto::RULE_SEMANTICS_BLACKLIST);
  rule.set_source_type(url_pattern_index::proto::SOURCE_TYPE_ANY);
  rule.set_element_types(url_pattern_index::proto::ELEMENT_TYPE_ALL);
  rule.set_url_pattern_type(
      url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING);
  rule.set_anchor_left(url_pattern_index::proto::ANCHOR_TYPE_NONE);
  rule.set_anchor_right(url_pattern_index::proto::ANCHOR_TYPE_BOUNDARY);
  rule.set_url_pattern(suffix);
  return rule;
}

void OnPublished(std::unique_ptr<base::ScopedTempDir> scoped_temp_dir,
                 subresource_filter::ContentRulesetService* service,
                 base::android::ScopedJavaGlobalRef<jobject> publisher) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Ensure the callback does not retain |publisher| by resetting it.
  service->SetRulesetPublishedCallbackForTesting(base::RepeatingClosure());
  scoped_temp_dir.reset();
  Java_TestSubresourceFilterPublisher_onRulesetPublished(
      base::android::AttachCurrentThread(), publisher);
}

}  // namespace

void JNI_TestSubresourceFilterPublisher_CreateAndPublishRulesetDisallowingSuffixForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& publisher_param,
    const base::android::JavaParamRef<jstring>& suffix) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::android::ScopedJavaGlobalRef<jobject> publisher;
  publisher.Reset(env, publisher_param);

  // Create the ruleset contents.
  std::string ruleset_contents_str;
  google::protobuf::io::StringOutputStream output(&ruleset_contents_str);
  subresource_filter::UnindexedRulesetWriter ruleset_writer(&output);
  ruleset_writer.AddUrlRule(
      CreateSuffixRule(base::android::ConvertJavaStringToUTF8(env, suffix)));
  ruleset_writer.Finish();
  auto* data = reinterpret_cast<const uint8_t*>(ruleset_contents_str.data());
  std::vector<uint8_t> ruleset_contents(data,
                                        data + ruleset_contents_str.size());

  // Create the ruleset directory and write the ruleset data into a file there.
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto scoped_temp_dir = std::make_unique<base::ScopedTempDir>();
  CHECK(scoped_temp_dir->CreateUniqueTempDir());
  base::FilePath ruleset_path = scoped_temp_dir->GetPath().AppendASCII("1");
  int ruleset_size_as_int = base::checked_cast<int>(ruleset_contents.size());
  CHECK_EQ(
      ruleset_size_as_int,
      base::WriteFile(ruleset_path,
                      reinterpret_cast<const char*>(ruleset_contents.data()),
                      ruleset_size_as_int));

  subresource_filter::ContentRulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();
  service->SetRulesetPublishedCallbackForTesting(base::BindRepeating(
      &OnPublished, base::Passed(&scoped_temp_dir), service, publisher));

  subresource_filter::UnindexedRulesetInfo unindexed_ruleset_info;
  unindexed_ruleset_info.content_version = "1";
  unindexed_ruleset_info.ruleset_path = ruleset_path;
  service->IndexAndStoreAndPublishRulesetIfNeeded(unindexed_ruleset_info);
}
