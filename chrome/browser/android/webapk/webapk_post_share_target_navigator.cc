// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_post_share_target_navigator.h"

#include <jni.h>

#include <sstream>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "chrome/android/chrome_jni_headers/WebApkPostShareTargetNavigator_jni.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

namespace webapk {

std::string PercentEscapeString(const std::string& unescaped_string) {
  std::ostringstream escaped_oss;
  for (size_t i = 0; i < unescaped_string.length(); ++i) {
    if (unescaped_string[i] == '"') {
      escaped_oss << "%22";
    } else if (unescaped_string[i] == 0x0a) {
      escaped_oss << "%0A";
    } else if (unescaped_string[i] == 0x0d) {
      escaped_oss << "%0D";
    } else {
      escaped_oss << unescaped_string[i];
    }
  }
  return escaped_oss.str();
}

void AddFile(const std::string& value_name,
             const std::string& file_uri,
             const std::string& file_name,
             const std::string& content_type,
             const std::string& boundary,
             scoped_refptr<network::ResourceRequestBody> result) {
  const char delimiter[] = "\r\n";
  const size_t delimiter_length = 2;
  std::string mime_header;
  // First line is the boundary.
  mime_header.append("--" + boundary + delimiter);
  // Next line is the Content-disposition.
  mime_header.append("Content-Disposition: form-data; name=\"" + value_name +
                     "\"");
  if (!file_name.empty()) {
    mime_header.append("; filename=\"" + file_name + "\"");
  }
  mime_header.append(delimiter);

  if (!content_type.empty()) {
    // If Content-type is specified, the next line is that.
    mime_header.append("Content-Type: " + content_type + delimiter);
  }
  // Leave an empty line before appending the file_uri.
  mime_header.append(delimiter);

  result->AppendBytes(mime_header.c_str(), mime_header.length());

  result->AppendFileRange(base::FilePath(file_uri), 0, -1, base::Time());

  result->AppendBytes(delimiter, delimiter_length);
}

void AddPlainText(const std::string& value_name,
                  const std::string& value,
                  const std::string& file_name,
                  const std::string& content_type,
                  const std::string& boundary,
                  scoped_refptr<network::ResourceRequestBody> result) {
  std::string item;
  if (file_name.empty()) {
    net::AddMultipartValueForUpload(value_name, value, boundary, content_type,
                                    &item);
  } else {
    net::AddMultipartValueForUploadWithFileName(value_name, file_name, value,
                                                boundary, content_type, &item);
  }
  result->AppendBytes(item.c_str(), item.length());
}

scoped_refptr<network::ResourceRequestBody> ComputeMultipartBody(
    const std::vector<std::string>& names,
    const std::vector<std::string>& values,
    const std::vector<bool>& is_value_file_uris,
    const std::vector<std::string>& filenames,
    const std::vector<std::string>& types,
    const std::string& boundary) {
  size_t num_files = names.size();
  if (num_files != values.size() || num_files != is_value_file_uris.size() ||
      num_files != filenames.size() || num_files != types.size()) {
    // The length of all arrays should always be the same for multipart POST.
    // This should never happen.
    return nullptr;
  }
  scoped_refptr<network::ResourceRequestBody> result =
      new network::ResourceRequestBody();

  for (size_t i = 0; i < num_files; i++) {
    if (is_value_file_uris[i]) {
      AddFile(PercentEscapeString(names[i]), values[i],
              PercentEscapeString(filenames[i]), types[i], boundary, result);
    } else {
      AddPlainText(PercentEscapeString(names[i]), values[i],
                   PercentEscapeString(filenames[i]), types[i], boundary,
                   result);
    }
  }

  std::string final_delimiter;
  net::AddMultipartFinalDelimiterForUpload(boundary, &final_delimiter);
  result->AppendBytes(final_delimiter.c_str(), final_delimiter.length());

  return result;
}

std::string ComputeUrlEncodedBody(const std::vector<std::string>& names,
                                  const std::vector<std::string>& values) {
  if (names.size() != values.size() || names.size() == 0)
    return "";
  std::ostringstream application_body_oss;
  application_body_oss << net::EscapeUrlEncodedData(names[0], true) << "="
                       << net::EscapeUrlEncodedData(values[0], true);
  for (size_t i = 1; i < names.size(); i++)
    application_body_oss << "&" << net::EscapeUrlEncodedData(names[i], true)
                         << "=" << net::EscapeUrlEncodedData(values[i], true);

  return application_body_oss.str();
}

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
  web_contents->OpenURL(open_url_params);
}
}  // namespace webapk

static void JNI_WebApkPostShareTargetNavigator_NativeLoadViewForShareTargetPost(
    JNIEnv* env,
    const jboolean java_is_multipart_encoding,
    const JavaParamRef<jobjectArray>& java_names,
    const JavaParamRef<jobjectArray>& java_values,
    const JavaParamRef<jbooleanArray>& java_is_value_file_uris,
    const JavaParamRef<jobjectArray>& java_filenames,
    const JavaParamRef<jobjectArray>& java_types,
    const JavaParamRef<jstring>& java_url,
    const JavaParamRef<jobject>& java_web_contents) {
  std::vector<std::string> names;
  std::vector<std::string> values;
  std::vector<std::string> filenames;
  std::vector<std::string> types;
  std::vector<bool> is_value_file_uris;

  bool is_multipart_encoding = static_cast<bool>(java_is_multipart_encoding);
  base::android::AppendJavaStringArrayToStringVector(env, java_names, &names);
  base::android::AppendJavaStringArrayToStringVector(env, java_values, &values);
  base::android::JavaBooleanArrayToBoolVector(env, java_is_value_file_uris,
                                              &is_value_file_uris);
  base::android::AppendJavaStringArrayToStringVector(env, java_filenames,
                                                     &filenames);
  base::android::AppendJavaStringArrayToStringVector(env, java_types, &types);

  GURL share_target_gurl(base::android::ConvertJavaStringToUTF8(java_url));

  scoped_refptr<network::ResourceRequestBody> post_data;
  std::string header_list;
  if (is_multipart_encoding) {
    std::string boundary = net::GenerateMimeMultipartBoundary();
    header_list = base::StringPrintf(
        "Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
    post_data = webapk::ComputeMultipartBody(names, values, is_value_file_uris,
                                             filenames, types, boundary);
  } else {
    std::string body = webapk::ComputeUrlEncodedBody(names, values);
    header_list = "Content-Type: application/x-www-form-urlencoded\r\n";
    post_data = network::ResourceRequestBody::CreateFromBytes(body.c_str(),
                                                              body.length());
  }

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  webapk::NavigateShareTargetPost(post_data, header_list, share_target_gurl,
                                  web_contents);
}
