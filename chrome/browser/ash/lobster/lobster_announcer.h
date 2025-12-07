// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_ANNOUNCER_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_ANNOUNCER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/lobster/announcement_view.h"
#include "ui/views/widget/widget_observer.h"

// An interface used to trigger ChromeVox announcements.
class LobsterAnnouncer {
 public:
  virtual void Announce(const std::u16string& message) = 0;
};

class LobsterLiveRegionAnnouncer : public LobsterAnnouncer {
 public:
  explicit LobsterLiveRegionAnnouncer(std::u16string_view name);

  virtual ~LobsterLiveRegionAnnouncer() = default;

  // LobsterAnnouncer overrides
  void Announce(const std::u16string& message) override;

 private:
  class LiveRegion : public views::WidgetObserver {
   public:
    explicit LiveRegion(std::u16string_view name);
    ~LiveRegion() override;

    // Triggers a ChromeVox announcement via the live region view.
    void Announce(const std::u16string& message);

    // WidgetObserver overrides
    void OnWidgetDestroying(views::Widget* widget) override;

   private:
    void CreateAnnouncementView();

    std::u16string announcement_view_name_;

    // Holds the view used to trigger announcements with ChromeVox. This is
    // a raw_ptr due to the lifetime of the instance being handled by the
    // DialogDelegateView the class inherits from.
    raw_ptr<AnnouncementView> announcement_view_ = nullptr;

    base::ScopedObservation<views::Widget, views::WidgetObserver> obs_{this};
  };

  LiveRegion live_region_;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_ANNOUNCER_H_
