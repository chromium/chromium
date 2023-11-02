// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_H_

#include <memory>
#include <string>
#include <vector>

namespace gfx {
class ImageSkia;
}

class StatusIcon;

// Provides a cross-platform interface to the system's status tray, and exposes
// APIs to add/remove icons to the tray and attach context menus.
class StatusTray {
 public:
  enum StatusIconType {
    NOTIFICATION_TRAY_ICON = 0,
    MEDIA_STREAM_CAPTURE_ICON,
    BACKGROUND_MODE_ICON,
    OTHER_ICON,
    NAMED_STATUS_ICON_COUNT
  };

  // Static factory method that is implemented separately for each platform to
  // produce the appropriate platform-specific instance. Returns NULL if this
  // platform does not support status icons.
  static std::unique_ptr<StatusTray> Create();

  StatusTray(const StatusTray&) = delete;
  StatusTray& operator=(const StatusTray&) = delete;

  virtual ~StatusTray();

  // Creates a new StatusIcon. The StatusTray retains ownership of the
  // StatusIcon. Returns NULL if the StatusIcon could not be created.
  StatusIcon* CreateStatusIcon(StatusIconType type,
                               const gfx::ImageSkia& image,
                               const std::u16string& tool_tip);

  // Removes |icon| from this status tray.
  void RemoveStatusIcon(StatusIcon* icon);

 protected:
  using StatusIcons = std::vector<std::unique_ptr<StatusIcon>>;

  StatusTray();

  // Factory method for creating a status icon for this platform.
  virtual std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) = 0;

  // Returns the list of active status icons so subclasses can operate on them.
  const StatusIcons& status_icons() const { return status_icons_; }

 private:
  // List containing all active StatusIcons. The icons are owned by this
  // StatusTray.
  StatusIcons status_icons_;
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_H_
