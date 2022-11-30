// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// String constants used when logging data in the extension activity log.

#include "chrome/browser/extensions/activity_log/activity_action_constants.h"

namespace activity_log_constants {

// Keys that may be used in the "other" attribute of an Action.
const char kActionDomVerb[] = "dom_verb";
const char kActionExtra[] = "extra";
const char kActionPrerender[] = "prerender";
const char kActionWebRequest[] = "web_request";

// A string used in place of the real URL when the URL is hidden because it is
// in an incognito window.  Extension activity logs mentioning kIncognitoUrl
// let the user know that an extension is manipulating incognito tabs without
// recording specific data about the pages.
const char kIncognitoUrl[] = "<incognito>";

// A string used as a placeholder for URLs which have been removed from the
// argument list and stored to the arg_url field.
const char kArgUrlPlaceholder[] = "<arg_url>";

}  // namespace activity_log_constants
