// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_STORAGE_TEST_UTILS_H_
#define CHROME_BROWSER_NET_STORAGE_TEST_UTILS_H_

#include <string>

class GURL;

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace storage {
namespace test {

// Helper to validate a given frame contains the |expected| contents as their
// document body.
void ExpectFrameContent(content::RenderFrameHost* frame,
                        const std::string& expected);
// Helper to validate a given for a given |context| and |host_url| that
// |expected| cookies are present.
void ExpectCookiesOnHost(content::BrowserContext* context,
                         const GURL& host_url,
                         const std::string& expected);

// Helpers to set and check various types of storage on a given frame. Typically
// used on page like //chrome/test/data/browsing_data/site_data.html
void SetStorageForFrame(content::RenderFrameHost* frame);
void SetStorageForWorker(content::RenderFrameHost* frame);
void ExpectStorageForFrame(content::RenderFrameHost* frame, bool expected);
void ExpectStorageForWorker(content::RenderFrameHost* frame, bool expected);

// Helpers to set and check various types of cross tab info for a given frame.
// Typically used on page like //chrome/test/data/browsing_data/site_data.html
void SetCrossTabInfoForFrame(content::RenderFrameHost* frame);
void ExpectCrossTabInfoForFrame(content::RenderFrameHost* frame, bool expected);

// Helper to request storage access for a frame using
// document.requestStorageAccess()
void RequestStorageAccessForFrame(content::RenderFrameHost* frame,
                                  bool should_resolve);
// Helper to validate if a frame currently has storage access using
// document.hasStorageAccess()
void CheckStorageAccessForFrame(content::RenderFrameHost* frame,
                                bool access_expected);

}  // namespace test
}  // namespace storage
#endif  // CHROME_BROWSER_NET_STORAGE_TEST_UTILS_H_
