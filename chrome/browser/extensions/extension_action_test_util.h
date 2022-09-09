// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_TEST_UTIL_H_

#include <stddef.h>

class Profile;
class ToolbarActionsModel;

namespace content {
class WebContents;
}

namespace extensions {
namespace extension_action_test_util {

// TODO(devlin): Should we also pull out methods to test browser actions?

// Returns the number of page actions that are visible in the given
// |web_contents|. Note that the "visible" here is somewhat inaccurate, since
// all extensions now have a permanent action. A better way of thinking of this
// would be "active".
// TODO(devlin): Rename this function.
size_t GetVisiblePageActionCount(content::WebContents* web_contents);

// Returns the total number of page actions (visible or not) for the given
// |web_contents|.
size_t GetTotalPageActionCount(content::WebContents* web_contents);

// Creates a new ToolbarActionsModel for the given |profile|, and associates
// it with the profile as a keyed service.
// This should only be used in unit tests (since it assumes the existence of
// a TestExtensionSystem), but if running a browser test, the model should
// already be created.
ToolbarActionsModel* CreateToolbarModelForProfile(Profile* profile);
// Like above, but doesn't run the ExtensionSystem::ready() task for the new
// model.
ToolbarActionsModel* CreateToolbarModelForProfileWithoutWaitingForReady(
    Profile* profile);

}  // namespace extension_action_test_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_TEST_UTIL_H_
