// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// String constants used when logging data in the extension activity log.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_ACTION_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_ACTION_CONSTANTS_H_

namespace activity_log_constants {

// Keys that may be used in the "other" attribute of an Action.
extern const char kActionDomVerb[];
extern const char kActionExtra[];
extern const char kActionPrerender[];
extern const char kActionWebRequest[];

// A string used in place of the real URL when the URL is hidden because it is
// in an incognito window.  Extension activity logs mentioning kIncognitoUrl
// let the user know that an extension is manipulating incognito tabs without
// recording specific data about the pages.
extern const char kIncognitoUrl[];

// A string used as a placeholder for URLs which have been removed from the
// argument list and stored to the arg_url field.
extern const char kArgUrlPlaceholder[];

}  // namespace activity_log_constants

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_ACTION_CONSTANTS_H_
