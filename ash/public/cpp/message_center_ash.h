// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MESSAGE_CENTER_ASH_H_
#define ASH_PUBLIC_CPP_MESSAGE_CENTER_ASH_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {

// Provide public access to quiet mode to code in //chrome that cannot
// directly depend on //ui/message_center.
class ASH_PUBLIC_EXPORT MessageCenterAsh {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnQuietModeChanged(bool in_quiet_mode) = 0;
  };

  // Returns the instance owned by Shell.
  static MessageCenterAsh* Get();

  // Initializes MessageCenterAsh for testing.
  static void SetForTesting(MessageCenterAsh* message_center);

  MessageCenterAsh(const MessageCenterAsh&) = delete;
  MessageCenterAsh& operator=(const MessageCenterAsh&) = delete;

  // This sets the internal state of the Quiet Mode and fires
  // observer on change for OnQuietModeChanged.
  virtual void SetQuietMode(bool in_quiet_mode) = 0;

  // Queries current notification Quiet Mode status.
  virtual bool IsQuietMode() const = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  MessageCenterAsh();
  virtual ~MessageCenterAsh();

  void NotifyOnQuietModeChanged(bool in_quiet_mode);

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MESSAGE_CENTER_ASH_H_
