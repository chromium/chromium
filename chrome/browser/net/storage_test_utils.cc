// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/storage_test_utils.h"

#include "content/public/test/browser_test_utils.h"

namespace storage::test {

const std::vector<std::string> kCookiesTypesForFrame{"Cookie", "CookieStore"};

const std::vector<std::string> kStorageTypesForFrame{
    "LocalStorage", "FileSystem", "FileSystemAccess", "SessionStorage",
    "IndexedDb",    "WebSql",     "CacheStorage",     "ServiceWorker"};

const std::vector<std::string> kStorageTypesForWorker{
    "WorkerFileSystemAccess", "WorkerCacheStorage", "WorkerIndexedDb"};

const std::vector<std::string> kCrossTabCommunicationTypes{
    "SharedWorker",
    "WebLock",
};

constexpr char kRequestStorageAccess[] =
    "document.requestStorageAccess()"
    "  .then(() => document.hasStorageAccess())"
    "  .catch(() => false);";

constexpr char kRequestStorageAccessFor[] =
    "document.requestStorageAccessFor($1).then("
    "  () => true,"
    "  () => false,"
    ");";

constexpr char kHasStorageAccess[] =
    "document.hasStorageAccess()"
    "  .catch(() => false);";

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
        content::EvalJs(frame, "set" + data_type + "()").ExtractBool();
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
        content::EvalJs(frame, "set" + data_type + "()").ExtractBool();
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
        content::EvalJs(frame, "has" + data_type + "();").ExtractBool();
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
        content::EvalJs(frame, "has" + data_type + "();").ExtractBool();
    expected_elts[data_type] = expected;
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected_elts));
}

void SetCrossTabInfoForFrame(content::RenderFrameHost* frame) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected;
  for (const auto& data_type : kCrossTabCommunicationTypes) {
    actual[data_type] =
        content::EvalJs(frame, "set" + data_type + "()").ExtractBool();
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
        content::EvalJs(frame, "has" + data_type + "();").ExtractBool();
    expected_elts[data_type] = expected;
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected_elts));
}

bool RequestAndCheckStorageAccessForFrame(content::RenderFrameHost* frame) {
  return content::EvalJs(frame, kRequestStorageAccess).ExtractBool();
}

bool RequestStorageAccessForOrigin(content::RenderFrameHost* frame,
                                   const std::string& origin) {
  return content::EvalJs(frame,
                         content::JsReplace(kRequestStorageAccessFor, origin))
      .ExtractBool();
}

bool HasStorageAccessForFrame(content::RenderFrameHost* frame) {
  return content::EvalJs(frame, kHasStorageAccess).ExtractBool();
}

std::string FetchWithCredentials(content::RenderFrameHost* frame,
                                 const GURL& url,
                                 const bool cors_enabled) {
  constexpr char script[] = R"(
      fetch($1, {method: 'GET', mode: $2, credentials: 'include'})
      .then((result) => result.text());
    )";
  const std::string mode = cors_enabled ? "cors" : "no-cors";
  return content::EvalJs(frame, content::JsReplace(script, url, mode))
      .ExtractString();
}

}  // namespace storage::test
