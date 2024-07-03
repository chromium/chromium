// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_STORAGE_TEST_UTILS_H_
#define CHROME_BROWSER_NET_STORAGE_TEST_UTILS_H_

#include <string>

#include "base/location.h"

class GURL;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace storage::test {

// Gets the text content of a given frame.
std::string GetFrameContent(content::RenderFrameHost* frame);

// Helpers to set and check various types of storage on a given frame. Typically
// used on page like //chrome/test/data/browsing_data/site_data.html
void SetStorageForFrame(content::RenderFrameHost* frame,
                        bool include_cookies,
                        bool expected_to_be_set = true,
                        const base::Location& location = FROM_HERE);
void SetStorageForWorker(content::RenderFrameHost* frame,
                         const base::Location& location = FROM_HERE);
void ExpectStorageForFrame(content::RenderFrameHost* frame,
                           bool expected,
                           const base::Location& location = FROM_HERE);
void ExpectStorageForWorker(content::RenderFrameHost* frame,
                            bool expected,
                            const base::Location& location = FROM_HERE);

// Helpers to set and check various types of cross tab info for a given frame.
// Typically used on page like //chrome/test/data/browsing_data/site_data.html
void SetCrossTabInfoForFrame(content::RenderFrameHost* frame,
                             const base::Location& location = FROM_HERE);
void ExpectCrossTabInfoForFrame(content::RenderFrameHost* frame,
                                bool expected,
                                const base::Location& location = FROM_HERE);

// Helper to request storage access for a frame using
// document.requestStorageAccess() and then get the value of
// document.hasStorageAccess(). If either call rejects, this helper DCHECKs.
bool RequestAndCheckStorageAccessForFrame(content::RenderFrameHost* frame,
                                          bool omit_user_gesture = false);
// Helper to request storage access for a frame using
// document.requestStorageAccess({estimate: true}) and then check the
// functionality of the handle.
bool RequestAndCheckStorageAccessBeyondCookiesForFrame(
    content::RenderFrameHost* frame);
// Helper to request storage access with a site override for a frame using
// document.requestStorageAccessFor(origin). Returns true if the promise
// resolves; false if it rejects.
bool RequestStorageAccessForOrigin(content::RenderFrameHost* frame,
                                   const std::string& origin,
                                   bool omit_user_gesture = false);
// Helper to see if a frame currently has storage access using
// document.hasStorageAccess(). Returns true if the promise resolves with a
// value of true; false otherwise.
bool HasStorageAccessForFrame(content::RenderFrameHost* frame);

// Helper to see if a credentialed fetch has cookies access via top-level
// storage access grants. Returns the content of the response if the promise
// resolves. `cors_enabled` sets fetch RequestMode to be "cors" or "no-cors".
std::string FetchWithCredentials(content::RenderFrameHost* frame,
                                 const GURL& url,
                                 const bool cors_enabled);

}  // namespace storage::test
#endif  // CHROME_BROWSER_NET_STORAGE_TEST_UTILS_H_
