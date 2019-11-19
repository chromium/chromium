// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_FAKE_DESKTOP_MEDIA_PICKER_FACTORY_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_FAKE_DESKTOP_MEDIA_PICKER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory.h"
#include "content/public/browser/desktop_media_id.h"

// Used in tests to supply fake picker.
class FakeDesktopMediaPickerFactory : public DesktopMediaPickerFactory {
 public:
  struct TestFlags {
    bool expect_screens = false;
    bool expect_windows = false;
    bool expect_tabs = false;
    bool expect_audio = false;
    content::DesktopMediaID selected_source;
    bool cancelled = false;

    // Following flags are set by FakeDesktopMediaPicker when it's created and
    // deleted.
    bool picker_created = false;
    bool picker_deleted = false;
  };

  FakeDesktopMediaPickerFactory();
  ~FakeDesktopMediaPickerFactory() override;

  //  |test_flags| are expected to outlive the factory.
  void SetTestFlags(TestFlags* test_flags, int tests_count);

  // DesktopMediaPickerFactory implementation
  std::unique_ptr<DesktopMediaPicker> CreatePicker() override;
  std::vector<std::unique_ptr<DesktopMediaList>> CreateMediaList(
      const std::vector<content::DesktopMediaID::Type>& types) override;

 private:
  TestFlags* test_flags_;
  int tests_count_;
  int current_test_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopMediaPickerFactory);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_FAKE_DESKTOP_MEDIA_PICKER_FACTORY_H_
