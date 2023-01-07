// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/storage_test_utils.h"

#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test_utils.h"

namespace storage::test {

const std::vector<std::string> kCookiesTypesForFrame{"Cookie", "CookieStore"};

const std::vector<std::string> kStorageTypesForFrame{
    "LocalStorage",   "FileSystem",    "FileSystemAccess",
    "SessionStorage", "IndexedDb",     "WebSql",
    "CacheStorage",   "ServiceWorker", "StorageFoundation"};

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

constexpr char kRequestStorageAccessForOrigin[] =
    "document.requestStorageAccessForOrigin('%s').then("
    "  () => { window.domAutomationController.send(true); },"
    "  () => { window.domAutomationController.send(false); },"
    ");";

constexpr char kHasStorageAccess[] =
    "document.hasStorageAccess().then("
    "  (result) => { window.domAutomationController.send(result); },"
    "  () => { window.domAutomationController.send(false); },"
    ");";

std::vector<std::string> GetStorageTypesForFrame(bool include_cookies) {
  std::vector<std::string> types(kStorageTypesForFrame);
  if (include_cookies) {
    types.insert(types.end(), kCookiesTypesForFrame.begin(),
                 kCookiesTypesForFrame.end());
  }
  return types;
}

std::string GetFrameContent(content::RenderFrameHost* frame) {
  return content::EvalJs(frame, "document.body.textContent").ExtractString();
}

void SetStorageForFrame(content::RenderFrameHost* frame, bool include_cookies) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected;
  for (const auto& data_type : GetStorageTypesForFrame(include_cookies)) {
    actual[data_type] =
        content::EvalJs(frame, "set" + data_type + "()",
                        content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
            .ExtractBool();
    if (frame->GetLastCommittedOrigin() !=
            frame->GetMainFrame()->GetLastCommittedOrigin() &&
        data_type == "WebSql") {
      // Third-party context WebSQL is disabled as of M97.
      expected[data_type] = false;
    } else {
      expected[data_type] = true;
    }
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected));
}

void SetStorageForWorker(content::RenderFrameHost* frame) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected;
  for (const auto& data_type : kStorageTypesForWorker) {
    actual[data_type] =
        content::EvalJs(frame, "set" + data_type + "()",
                        content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
            .ExtractBool();
    expected[data_type] = true;
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected));
}

void ExpectStorageForFrame(content::RenderFrameHost* frame,
                           bool include_cookies,
                           bool expected) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected_elts;
  for (const auto& data_type : GetStorageTypesForFrame(include_cookies)) {
    actual[data_type] =
        content::EvalJs(frame, "has" + data_type + "();",
                        content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
            .ExtractBool();
    if (frame->GetLastCommittedOrigin() !=
            frame->GetMainFrame()->GetLastCommittedOrigin() &&
        data_type == "WebSql") {
      // Third-party context WebSQL is disabled as of M97.
      expected_elts[data_type] = false;
    } else {
      expected_elts[data_type] = expected;
    }
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected_elts));
}

void ExpectStorageForWorker(content::RenderFrameHost* frame, bool expected) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected_elts;
  for (const auto& data_type : kStorageTypesForWorker) {
    actual[data_type] =
        content::EvalJs(frame, "has" + data_type + "();",
                        content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
            .ExtractBool();
    expected_elts[data_type] = expected;
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected_elts));
}

void SetCrossTabInfoForFrame(content::RenderFrameHost* frame) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected;
  for (const auto& data_type : kCrossTabCommunicationTypes) {
    actual[data_type] =
        content::EvalJs(frame, "set" + data_type + "()",
                        content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
            .ExtractBool();
    expected[data_type] = true;
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected));
}

void ExpectCrossTabInfoForFrame(content::RenderFrameHost* frame,
                                bool expected) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected_elts;
  for (const auto& data_type : kCrossTabCommunicationTypes) {
    actual[data_type] =
        content::EvalJs(frame, "has" + data_type + "();",
                        content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
            .ExtractBool();
    expected_elts[data_type] = expected;
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected_elts));
}

bool RequestStorageAccessForFrame(content::RenderFrameHost* frame) {
  return content::EvalJs(frame, kRequestStorageAccess,
                         content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
      .ExtractBool();
}

bool RequestStorageAccessForOrigin(content::RenderFrameHost* frame,
                                   const std::string& origin) {
  return content::EvalJs(
             frame,
             base::StringPrintf(kRequestStorageAccessForOrigin, origin.c_str()),
             content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
      .ExtractBool();
}

bool HasStorageAccessForFrame(content::RenderFrameHost* frame) {
  return content::EvalJs(frame, kHasStorageAccess,
                         content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
      .ExtractBool();
}

}  // namespace storage::test
