// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MEDIA_PARSER_BRIDGE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MEDIA_PARSER_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/download/android/download_media_parser.h"

class DownloadMediaParser;

// The JNI bridge that uses DownloadMediaParser to parse local media file. The
// bridge is owned by the Java side.
class DownloadMediaParserBridge {
 public:
  DownloadMediaParserBridge(
      const std::string& mime_type,
      const base::FilePath& file_path,
      DownloadMediaParser::ParseCompleteCB parse_complete_cb);
  ~DownloadMediaParserBridge();

  void Destroy(JNIEnv* env, jobject obj);
  void Start(JNIEnv* env, jobject obj);

 private:
  // The media parser that does actual jobs in a sandboxed process.
  std::unique_ptr<DownloadMediaParser> parser_;
  DownloadMediaParser::ParseCompleteCB parse_complete_cb_;

  DISALLOW_COPY_AND_ASSIGN(DownloadMediaParserBridge);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MEDIA_PARSER_BRIDGE_H_
