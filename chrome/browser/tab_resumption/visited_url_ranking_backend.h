// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_RESUMPTION_VISITED_URL_RANKING_BACKEND_H_
#define CHROME_BROWSER_TAB_RESUMPTION_VISITED_URL_RANKING_BACKEND_H_

#include <jni.h>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"

namespace tab_resumption::jni {

// Provides the fetch and rank services of the Tab resumption backend to
// Java.
class VisitedUrlRankingBackend {
 public:
  explicit VisitedUrlRankingBackend(Profile* profile);

  VisitedUrlRankingBackend(const VisitedUrlRankingBackend&) = delete;
  VisitedUrlRankingBackend& operator=(const VisitedUrlRankingBackend&) = delete;

  ~VisitedUrlRankingBackend();

  void Destroy(JNIEnv* env);

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace tab_resumption::jni

#endif  // CHROME_BROWSER_TAB_RESUMPTION_VISITED_URL_RANKING_BACKEND_H_
