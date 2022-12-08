// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_TEST_CHROME_APP_LIST_TEST_SUPPORT_H_
#define CHROME_BROWSER_ASH_APP_LIST_TEST_CHROME_APP_LIST_TEST_SUPPORT_H_

class AppListClientImpl;
class AppListModelUpdater;
class Profile;

namespace test {

// Gets the model updater keyed to the profile currently associated with
// |service|.
AppListModelUpdater* GetModelUpdater(AppListClientImpl* client);

// Gets a client to control AppList or get its status.
AppListClientImpl* GetAppListClient();

// Creates a second profile in a nested run loop for testing the app list.
Profile* CreateSecondProfileAsync();

// Creates |n| app items with dummy data and adds to the current app-list
// client. New app items are appended to the end of the app list.
void PopulateDummyAppListItems(int n);

}  // namespace test

#endif  // CHROME_BROWSER_ASH_APP_LIST_TEST_CHROME_APP_LIST_TEST_SUPPORT_H_
