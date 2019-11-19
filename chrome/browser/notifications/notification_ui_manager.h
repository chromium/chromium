// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/notifications/notification_common.h"

typedef void* ProfileID;

class GURL;
class Profile;

namespace message_center {
class Notification;
}

// This interface is used to manage the UI surfaces for non-native desktop
// notifications (hence, this is not used by Chrome OS and is not used for
// Windows Action Center notifications). There is just one instance for all
// profiles. This represents the middle layer of notification and it's aware of
// profile. It identifies a notification by the id string and a profile, hence
// two notifications from two different profiles, even though they may have
// identical ids, will not be considered the same notification. This interface
// will generate a new id behind the scene based on the id string and the
// profile's characteristics for each notification and use this new id to call
// lower layer MessageCenter interface which is profile agnostic. Therefore the
// ids passed into this interface are not the same as those passed into the
// MessageCenter interface.
class NotificationUIManager {
 public:
  // Convert a profile pointer into an opaque profile id, which can be safely
  // used by FindById() and CancelById() even after a profile may have been
  // destroyed.
  static ProfileID GetProfileID(Profile* profile) {
    return static_cast<ProfileID>(profile);
  }

  virtual ~NotificationUIManager() {}

  // Creates an initialized UI manager.
  static std::unique_ptr<NotificationUIManager> Create();

  // Adds a notification to be displayed.
  virtual void Add(const message_center::Notification& notification,
                   Profile* profile) = 0;

  // Updates the given notification, if it already exists.Returns true for
  // update and false to report a no-op.
  virtual bool Update(const message_center::Notification& notification,
                      Profile* profile) = 0;

  // Returns the pointer to a notification if it match the supplied ID, either
  // currently displayed or in the queue.
  // This function can be bound for delayed execution, where a profile pointer
  // may not be valid. Hence caller needs to call the static GetProfileID(...)
  // function to turn a profile pointer into a profile id and pass that in.
  virtual const message_center::Notification* FindById(
      const std::string& delegate_id,
      ProfileID profile_id) const = 0;

  // Removes any notifications matching the supplied ID, either currently
  // displayed or in the queue.  Returns true if anything was removed.
  // This function can be bound for delayed execution, where a profile pointer
  // may not be valid. Hence caller needs to call the static GetProfileID(...)
  // function to turn a profile pointer into a profile id and pass that in.
  virtual bool CancelById(const std::string& delegate_id,
                          ProfileID profile_id) = 0;

  // Returns the set of all delegate IDs for notifications from |profile_id|.
  virtual std::set<std::string> GetAllIdsByProfile(ProfileID profile_id) = 0;

  // Removes notifications matching the |source_origin| (which could be an
  // extension ID). Returns true if anything was removed.
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin) = 0;

  // Cancels all pending notifications and closes anything currently showing.
  // Used when the app is terminating.
  virtual void CancelAll() = 0;

  // Cancels all pending notifications and closes anything currently showing.
  // After this is called, no new notifications can be added. Used when the app
  // is terminating.
  virtual void StartShutdown() = 0;

 protected:
  NotificationUIManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationUIManager);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_H_
