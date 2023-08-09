// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PDF_EXPORTER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PDF_EXPORTER_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

#include "printing/page_range.h"

namespace content {
class WebContents;
}

namespace printing {
class PrintSettings;
}

namespace android_webview {

// Native companion to Java AwPdfExporter.
// Owned by native AwContents, which lazy-instantiates this object when
// instructed to by the Java side.
//
// The Java AwPdfExporter holds a pointer to this native component but is not
// responsible for its lifetime.
// The Java AwPdfExporter is similarly owned by the Java AwContents.
//
// Lifetime: WebView
class AwPdfExporter {
 public:
  AwPdfExporter(JNIEnv* env,
                const base::android::JavaRef<jobject>& obj,
                content::WebContents* web_contents);

  AwPdfExporter(const AwPdfExporter&) = delete;
  AwPdfExporter& operator=(const AwPdfExporter&) = delete;

  ~AwPdfExporter();

  void ExportToPdf(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   int fd,
                   const base::android::JavaParamRef<jintArray>& pages,
                   const base::android::JavaParamRef<jobject>& cancel_signal);

 private:
  std::unique_ptr<printing::PrintSettings> CreatePdfSettings(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj,
      const printing::PageRanges& page_ranges);
  void DidExportPdf(int page_count);

  JavaObjectWeakGlobalRef java_ref_;
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_PDF_EXPORTER_H_
