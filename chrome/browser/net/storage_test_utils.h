// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_STORAGE_TEST_UTILS_H_
#define CHROME_BROWSER_NET_STORAGE_TEST_UTILS_H_

#include <string>

class GURL;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace storage {
namespace test {

// Gets the text content of a given frame.
std::string GetFrameContent(content::RenderFrameHost* frame);

// Helpers to set and check various types of storage on a given frame. Typically
// used on page like //chrome/test/data/browsing_data/site_data.html
void SetStorageForFrame(content::RenderFrameHost* frame, bool include_cookies);
void SetStorageForWorker(content::RenderFrameHost* frame);
void ExpectStorageForFrame(content::RenderFrameHost* frame,
                           bool include_cookies,
                           bool expected);
void ExpectStorageForWorker(content::RenderFrameHost* frame, bool expected);

// Helpers to set and check various types of cross tab info for a given frame.
// Typically used on page like //chrome/test/data/browsing_data/site_data.html
void SetCrossTabInfoForFrame(content::RenderFrameHost* frame);
void ExpectCrossTabInfoForFrame(content::RenderFrameHost* frame, bool expected);

// Helper to request storage access for a frame using
// document.requestStorageAccess(). Returns true if the promise resolves; false
// if it rejects.
bool RequestStorageAccessForFrame(content::RenderFrameHost* frame);
// Helper to request storage access with a site override for a frame using
// document.requestStorageAccessForOrigin(origin). Returns true if the promise
// resolves; false if it rejects.
bool RequestStorageAccessForOrigin(content::RenderFrameHost* frame,
                                   const std::string& origin);
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

}  // namespace test
}  // namespace storage
#endif  // CHROME_BROWSER_NET_STORAGE_TEST_UTILS_H_
