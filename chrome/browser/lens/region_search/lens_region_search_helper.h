// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_HELPER_H_
#define CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_HELPER_H_

class Browser;
class GURL;
class Profile;
class TemplateURLService;

namespace lens {

bool IsRegionSearchEnabled(Browser* browser,
                           Profile* profile,
                           TemplateURLService* service,
                           const GURL& url);

bool IsInProgressiveWebApp(Browser* browser);

}  // namespace lens

#endif  // CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_HELPER_H_
