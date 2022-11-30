// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_icon.h"

#include "base/compiler_specific.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notifier_id.h"

class MockStatusIconObserver : public StatusIconObserver {
 public:
  MOCK_METHOD0(OnStatusIconClicked, void());
};

// Define pure virtual functions so we can test base class functionality.
class TestStatusIcon : public StatusIcon {
 public:
  TestStatusIcon() {}
  void SetImage(const gfx::ImageSkia& image) override {}
  void SetToolTip(const std::u16string& tool_tip) override {}
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override {}
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override {}
};

TEST(StatusIconTest, ObserverAdd) {
  // Make sure that observers are invoked when we click items.
  TestStatusIcon icon;
  MockStatusIconObserver observer, observer2;
  EXPECT_CALL(observer, OnStatusIconClicked()).Times(2);
  EXPECT_CALL(observer2, OnStatusIconClicked());
  icon.AddObserver(&observer);
  icon.DispatchClickEvent();
  icon.AddObserver(&observer2);
  icon.DispatchClickEvent();
  icon.RemoveObserver(&observer);
  icon.RemoveObserver(&observer2);
}

TEST(StatusIconTest, ObserverRemove) {
  // Make sure that observers are no longer invoked after they are removed.
  TestStatusIcon icon;
  MockStatusIconObserver observer;
  EXPECT_CALL(observer, OnStatusIconClicked());
  icon.AddObserver(&observer);
  icon.DispatchClickEvent();
  icon.RemoveObserver(&observer);
  icon.DispatchClickEvent();
}
