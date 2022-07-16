// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/storage_test_utils.h"

#include "content/public/test/browser_test_utils.h"

namespace storage {
namespace test {

const std::vector<std::string> kStorageTypesForFrame{
    "Cookie",         "LocalStorage", "FileSystem",       "FileSystemAccess",
    "SessionStorage", "IndexedDb",    "WebSql",           "CacheStorage",
    "ServiceWorker",  "CookieStore",  "StorageFoundation"};

const std::vector<std::string> kStorageTypesForWorker{
    "WorkerFileSystemAccess", "WorkerCacheStorage", "WorkerIndexedDb",
    "WorkerStorageFoundation"};

const std::vector<std::string> kCrossTabCommunicationTypes{
    "SharedWorker",
    "WebLock",
};

constexpr char kRequestStorageAccess[] =
    "document.requestStorageAccess().then("
    "  () => { window.domAutomationController.send(true); },"
    "  () => { window.domAutomationController.send(false); },"
    ");";

constexpr char kHasStorageAccess[] =
    "document.hasStorageAccess().then("
    "  (result) => { window.domAutomationController.send(result); },"
    "  () => { window.domAutomationController.send(false); },"
    ");";

void ExpectFrameContent(content::RenderFrameHost* frame,
                        const std::string& expected) {
  EXPECT_EQ(expected, content::EvalJs(frame, "document.body.textContent"));
}

void ExpectCookiesOnHost(content::BrowserContext* context,
                         const GURL& host_url,
                         const std::string& expected) {
  EXPECT_EQ(expected, content::GetCookies(context, host_url));
}

void SetStorageForFrame(content::RenderFrameHost* frame) {
  for (const auto& data_type : kStorageTypesForFrame) {
    bool data = content::EvalJs(frame, "set" + data_type + "()",
                                content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                    .ExtractBool();
    if (frame->GetLastCommittedOrigin() !=
            frame->GetMainFrame()->GetLastCommittedOrigin() &&
        data_type == "WebSql") {
      // Third-party context WebSQL is disabled as of M97.
      EXPECT_FALSE(data) << "SetStorageForFrame for " << data_type;
    } else {
      EXPECT_TRUE(data) << "SetStorageForFrame for " << data_type;
    }
  }
}

void SetStorageForWorker(content::RenderFrameHost* frame) {
  for (const auto& data_type : kStorageTypesForWorker) {
    bool data = content::EvalJs(frame, "set" + data_type + "()",
                                content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                    .ExtractBool();
    EXPECT_TRUE(data) << "SetStorageForWorker for " << data_type;
  }
}

void ExpectStorageForFrame(content::RenderFrameHost* frame, bool expected) {
  for (const auto& data_type : kStorageTypesForFrame) {
    bool data = content::EvalJs(frame, "has" + data_type + "();",
                                content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                    .ExtractBool();
    if (frame->GetLastCommittedOrigin() !=
            frame->GetMainFrame()->GetLastCommittedOrigin() &&
        data_type == "WebSql") {
      // Third-party context WebSQL is disabled as of M97.
      EXPECT_FALSE(data) << "ExpectStorageForFrame for " << data_type;
    } else {
      EXPECT_EQ(expected, data) << "ExpectStorageForFrame for " << data_type;
    }
  }
}

void ExpectStorageForWorker(content::RenderFrameHost* frame, bool expected) {
  for (const auto& data_type : kStorageTypesForWorker) {
    bool data = content::EvalJs(frame, "has" + data_type + "();",
                                content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                    .ExtractBool();
    EXPECT_EQ(expected, data) << "ExpectStorageForWorker for " << data_type;
  }
}

void SetCrossTabInfoForFrame(content::RenderFrameHost* frame) {
  for (const auto& data_type : kCrossTabCommunicationTypes) {
    bool data = content::EvalJs(frame, "set" + data_type + "()",
                                content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                    .ExtractBool();
    EXPECT_TRUE(data) << data_type;
  }
}

void ExpectCrossTabInfoForFrame(content::RenderFrameHost* frame,
                                bool expected) {
  for (const auto& data_type : kCrossTabCommunicationTypes) {
    bool data = content::EvalJs(frame, "has" + data_type + "();",
                                content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                    .ExtractBool();
    EXPECT_EQ(expected, data) << data_type;
  }
}

void RequestStorageAccessForFrame(content::RenderFrameHost* frame,
                                  bool should_resolve) {
  bool data = content::EvalJs(frame, kRequestStorageAccess,
                              content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                  .ExtractBool();
  EXPECT_EQ(should_resolve, data) << "document.requestStorageAccess()";
}

void CheckStorageAccessForFrame(content::RenderFrameHost* frame,
                                bool access_expected) {
  bool data = content::EvalJs(frame, kHasStorageAccess,
                              content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                  .ExtractBool();
  EXPECT_EQ(access_expected, data) << "document.hasStorageAccess()";
}

}  // namespace test
}  // namespace storage
