// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_post_share_target_navigator.h"

#include <jni.h>

#include <sstream>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "chrome/browser/web_share_target/target_util.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebApkPostShareTargetNavigator_jni.h"

using base::android::JavaParamRef;

namespace webapk {

void NavigateShareTargetPost(
    const scoped_refptr<network::ResourceRequestBody>& post_data,
    const std::string& header_list,
    const GURL& share_target_gurl,
    content::WebContents* web_contents) {
  content::OpenURLParams open_url_params(
      share_target_gurl, content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      false /* is_renderer_initiated */);
  open_url_params.post_data = post_data;
  open_url_params.extra_headers = header_list;
  web_contents->OpenURL(open_url_params, /*navigation_handle_callback=*/{});
}
}  // namespace webapk

static void JNI_WebApkPostShareTargetNavigator_NativeLoadViewForShareTargetPost(
    JNIEnv* env,
    const jboolean java_is_multipart_encoding,
    std::vector<std::string>& names,
    std::vector<std::string>& values,
    const JavaParamRef<jbooleanArray>& java_is_value_file_uris,
    std::vector<std::string>& filenames,
    std::vector<std::string>& types,
    std::string& url,
    const JavaParamRef<jobject>& java_web_contents) {
  std::vector<bool> is_value_file_uris;

  bool is_multipart_encoding = static_cast<bool>(java_is_multipart_encoding);
  base::android::JavaBooleanArrayToBoolVector(env, java_is_value_file_uris,
                                              &is_value_file_uris);

  GURL share_target_gurl(url);

  scoped_refptr<network::ResourceRequestBody> post_data;
  std::string header_list;
  if (is_multipart_encoding) {
    std::string boundary = net::GenerateMimeMultipartBoundary();
    header_list = base::StringPrintf(
        "Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
    post_data = web_share_target::ComputeMultipartBody(
        names, values, is_value_file_uris, filenames, types, boundary);
  } else {
    std::string body = web_share_target::ComputeUrlEncodedBody(names, values);
    header_list = "Content-Type: application/x-www-form-urlencoded\r\n";
    post_data = network::ResourceRequestBody::CreateFromBytes(body.c_str(),
                                                              body.length());
  }

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  webapk::NavigateShareTargetPost(post_data, header_list, share_target_gurl,
                                  web_contents);
}
