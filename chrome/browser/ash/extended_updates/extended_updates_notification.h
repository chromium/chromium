// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_NOTIFICATION_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

// Class that constructs, shows, and handles the Extended Updates notification.
// This class manages its own ownership. It stays alive while the notification
// is shown, and self-destructs when the notification is closed.
class ExtendedUpdatesNotification : public message_center::NotificationObserver,
                                    public ProfileObserver {
 public:
  // Maps notification buttons to their ordered indices.
  enum class IndexedButton : int {
    kSetUp = 0,
    kLearnMore = 1,
  };

  static constexpr char kNotificationId[] = "ash.extended_updates.available";

  // Creates a new notification handler.
  static base::WeakPtr<ExtendedUpdatesNotification> Create(Profile* profile);

  ExtendedUpdatesNotification(const ExtendedUpdatesNotification&) = delete;
  ExtendedUpdatesNotification& operator=(const ExtendedUpdatesNotification&) =
      delete;
  ~ExtendedUpdatesNotification() override;

  // Shows the notification.
  void Show();

  // message_center::NotificationObserver overrides.
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

  // ProfileObserver overrides.
  void OnProfileWillBeDestroyed(Profile* profile) override;

  base::WeakPtr<ExtendedUpdatesNotification> GetWeakPtr();

 protected:
  explicit ExtendedUpdatesNotification(Profile* profile);

  virtual void ShowExtendedUpdatesDialog();
  virtual void OpenLearnMoreUrl();

 private:
  base::WeakPtr<ExtendedUpdatesNotification> BuildAndDisplayNotification(
      Profile* profile);

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtrFactory<ExtendedUpdatesNotification> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_NOTIFICATION_H_
