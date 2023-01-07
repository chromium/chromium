// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_OBSERVER_H_

class SupervisedUserServiceObserver {
 public:
  // Called whenever the URL filter is updated, e.g. a manual exception or a
  // content pack is added, or the default fallback behavior is changed.
  virtual void OnURLFilterChanged() {}

  // Called when information about the supervised user's custodian is changed,
  // e.g. the display name.
  virtual void OnCustodianInfoChanged() {}

 protected:
  virtual ~SupervisedUserServiceObserver() {}
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_OBSERVER_H_
