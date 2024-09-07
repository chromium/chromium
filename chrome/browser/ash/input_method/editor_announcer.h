// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_ANNOUNCER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_ANNOUNCER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/input_method/announcement_view.h"
#include "ui/views/widget/widget_observer.h"

namespace ash::input_method {

// An interface used to trigger ChromeVox announcements.
class EditorAnnouncer {
 public:
  virtual void Announce(const std::u16string& message) = 0;
};

class EditorLiveRegionAnnouncer : public EditorAnnouncer {
 public:
  explicit EditorLiveRegionAnnouncer(std::u16string_view name);

  // EditorAnnouncer overrides
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
    raw_ptr<ui::ime::AnnouncementView> announcement_view_ = nullptr;

    base::ScopedObservation<views::Widget, views::WidgetObserver> obs_{this};
  };

  LiveRegion live_region_;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_ANNOUNCER_H_
