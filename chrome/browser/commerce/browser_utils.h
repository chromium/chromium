// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_BROWSER_UTILS_H_
#define CHROME_BROWSER_COMMERCE_BROWSER_UTILS_H_
#include <vector>

class Profile;
class GURL;
namespace content {
class WebContents;
}  // namespace content

namespace commerce {
// Minimum number of tabsrequired for product specifications experience menu.
extern const int kProductSpecificationsMinTabsCount;

// Extracts the list of URLs that can be used for ProductSpecs experience from
// |web_contents|
const std::vector<GURL> GetListOfProductSpecsEligibleUrls(
    const std::vector<content::WebContents*> web_contents_list);

// Returns true if |web_contents_list| has enough eligible URLs for
// ProductSpecs.
bool IsWebContentsListEligibleForProductSpecs(
    const std::vector<content::WebContents*> web_contents_list);

// Returns true the multi-selection context menu should be enabled for |profile|
// and |web_contents|
bool IsProductSpecsMultiSelectMenuEnabled(Profile* profile,
                                          content::WebContents* web_contents);

}  // namespace commerce

#endif  // CHROME_BROWSER_COMMERCE_BROWSER_UTILS_H_
