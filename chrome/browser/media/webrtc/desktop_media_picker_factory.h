// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_FACTORY_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "content/public/browser/desktop_media_id.h"

// Interface for factory creating DesktopMediaList and DesktopMediaPicker
// instances.
class DesktopMediaPickerFactory {
 public:
  virtual ~DesktopMediaPickerFactory();

  virtual std::unique_ptr<DesktopMediaPicker> CreatePicker() = 0;
  virtual std::vector<std::unique_ptr<DesktopMediaList>> CreateMediaList(
      const std::vector<content::DesktopMediaID::Type>& types) = 0;

 protected:
  DesktopMediaPickerFactory();

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopMediaPickerFactory);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_FACTORY_H_
