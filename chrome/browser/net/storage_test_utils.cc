// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/storage_test_utils.h"

#include "content/public/test/browser_test_utils.h"

namespace storage::test {

const std::vector<std::string> kCookiesTypesForFrame{"Cookie", "CookieStore"};

const std::vector<std::string> kStorageTypesForFrame{
    "LocalStorage", "FileSystem", "FileSystemAccess", "SessionStorage",
    "IndexedDb",     "CacheStorage",     "ServiceWorker",
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/333756088): WebSQL is disabled everywhere by default as of
    // M119 (crbug/695592) except on Android WebView. This is enabled for
    // Android only to indirectly cover WebSQL deletions on WebView.
    "WebSql",
#endif
};

const std::vector<std::string> kStorageTypesForWorker{
    "WorkerFileSystemAccess", "WorkerCacheStorage", "WorkerIndexedDb"};

const std::vector<std::string> kCrossTabCommunicationTypes{
    "SharedWorker",
    "WebLock",
};

constexpr char kRequestStorageAccess[] =
    "document.requestStorageAccess()"
    "  .then(() => document.hasStorageAccess())";

constexpr char kRequestStorageAccessBeyondCookies[] =
    "document.requestStorageAccess({estimate: true}).then((handle) => "
    "handle.estimate().then(() => true, () => false), () => false)";

constexpr char kRequestStorageAccessFor[] =
    "document.requestStorageAccessFor($1)";

constexpr char kHasStorageAccess[] = "document.hasStorageAccess()";

std::vector<std::string> GetStorageTypesForFrame(bool include_cookies) {
  std::vector<std::string> types(kStorageTypesForFrame);
  if (include_cookies) {
    types.insert(types.end(), kCookiesTypesForFrame.begin(),
                 kCookiesTypesForFrame.end());
  }
  return types;
}

std::string GetFrameContent(content::RenderFrameHost* frame) {
  return content::EvalJs(frame, "document.body.textContent",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE)
      .ExtractString();
}

void SetStorageForFrame(content::RenderFrameHost* frame,
                        bool include_cookies,
                        bool expected_to_be_set,
                        const base::Location& location) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected;
  for (const auto& data_type : GetStorageTypesForFrame(include_cookies)) {
    actual[data_type] = content::EvalJs(frame, "set" + data_type + "()",
                                        content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                            .ExtractBool();
    if (frame->GetLastCommittedOrigin() !=
            frame->GetMainFrame()->GetLastCommittedOrigin() &&
        data_type == "WebSql") {
      // Third-party context WebSQL is disabled as of M97.
      expected[data_type] = false;
    } else {
      expected[data_type] = expected_to_be_set;
    }
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected))
      << "(expected at " << location.ToString() << ")";
}

void SetStorageForWorker(content::RenderFrameHost* frame,
                         const base::Location& location) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected;
  for (const auto& data_type : kStorageTypesForWorker) {
    actual[data_type] = content::EvalJs(frame, "set" + data_type + "()",
                                        content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                            .ExtractBool();
    expected[data_type] = true;
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected))
      << "(expected at " << location.ToString() << ")";
}

void ExpectStorageForFrame(content::RenderFrameHost* frame,
                           bool expected,
                           const base::Location& location) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected_elts;
  for (const auto& data_type : GetStorageTypesForFrame(false)) {
    actual[data_type] = content::EvalJs(frame, "has" + data_type + "();",
                                        content::EXECUTE_SCRIPT_NO_USER_GESTURE)
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
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected_elts))
      << "(expected at " << location.ToString() << ")";
}

void ExpectStorageForWorker(content::RenderFrameHost* frame,
                            bool expected,
                            const base::Location& location) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected_elts;
  for (const auto& data_type : kStorageTypesForWorker) {
    actual[data_type] = content::EvalJs(frame, "has" + data_type + "();",
                                        content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                            .ExtractBool();
    expected_elts[data_type] = expected;
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected_elts))
      << "(expected at " << location.ToString() << ")";
}

void SetCrossTabInfoForFrame(content::RenderFrameHost* frame,
                             const base::Location& location) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected;
  for (const auto& data_type : kCrossTabCommunicationTypes) {
    actual[data_type] = content::EvalJs(frame, "set" + data_type + "()",
                                        content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                            .ExtractBool();
    expected[data_type] = true;
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected))
      << "(expected at " << location.ToString() << ")";
}

void ExpectCrossTabInfoForFrame(content::RenderFrameHost* frame,
                                bool expected,
                                const base::Location& location) {
  base::flat_map<std::string, bool> actual;
  base::flat_map<std::string, bool> expected_elts;
  for (const auto& data_type : kCrossTabCommunicationTypes) {
    actual[data_type] = content::EvalJs(frame, "has" + data_type + "();",
                                        content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                            .ExtractBool();
    expected_elts[data_type] = expected;
  }
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected_elts))
      << "(expected at " << location.ToString() << ")";
}

bool RequestAndCheckStorageAccessForFrame(content::RenderFrameHost* frame,
                                          bool omit_user_gesture) {
  int options = content::EXECUTE_SCRIPT_DEFAULT_OPTIONS;
  if (omit_user_gesture) {
    options |= content::EXECUTE_SCRIPT_NO_USER_GESTURE;
  }
  return content::EvalJs(frame, kRequestStorageAccess, options).ExtractBool();
}

bool RequestAndCheckStorageAccessBeyondCookiesForFrame(
    content::RenderFrameHost* frame) {
  return content::EvalJs(frame, kRequestStorageAccessBeyondCookies)
      .ExtractBool();
}

bool RequestStorageAccessForOrigin(content::RenderFrameHost* frame,
                                   const std::string& origin,
                                   bool omit_user_gesture) {
  int options = content::EXECUTE_SCRIPT_DEFAULT_OPTIONS;
  if (omit_user_gesture) {
    options |= content::EXECUTE_SCRIPT_NO_USER_GESTURE;
  }
  return content::ExecJs(
      frame, content::JsReplace(kRequestStorageAccessFor, origin), options);
}

bool HasStorageAccessForFrame(content::RenderFrameHost* frame) {
  return content::EvalJs(frame, kHasStorageAccess,
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE)
      .ExtractBool();
}

std::string FetchWithCredentials(content::RenderFrameHost* frame,
                                 const GURL& url,
                                 const bool cors_enabled) {
  constexpr char script[] = R"(
      fetch($1, {method: 'GET', mode: $2, credentials: 'include'})
      .then((result) => result.text());
    )";
  const std::string mode = cors_enabled ? "cors" : "no-cors";
  return content::EvalJs(frame, content::JsReplace(script, url, mode),
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE)
      .ExtractString();
}

}  // namespace storage::test
