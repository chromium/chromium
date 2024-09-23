// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_FAKE_DESKTOP_MEDIA_PICKER_FACTORY_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_FAKE_DESKTOP_MEDIA_PICKER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory.h"
#include "content/public/browser/desktop_media_id.h"

class FakeDesktopMediaPicker;

// Used in tests to supply fake picker.
class FakeDesktopMediaPickerFactory : public DesktopMediaPickerFactory {
 public:
  // TODO(crbug.com/40169647): Make this less error prone - use WithX() methods.
  struct TestFlags {
    bool expect_screens = false;
    bool expect_windows = false;
    bool expect_tabs = false;
    bool expect_current_tab = false;
    bool expect_audio = false;
    content::DesktopMediaID selected_source;
    bool cancelled = false;

    // Following flags are set by FakeDesktopMediaPicker when it's created and
    // deleted.
    bool picker_created = false;
    bool picker_deleted = false;
  };

  FakeDesktopMediaPickerFactory();

  FakeDesktopMediaPickerFactory(const FakeDesktopMediaPickerFactory&) = delete;
  FakeDesktopMediaPickerFactory& operator=(
      const FakeDesktopMediaPickerFactory&) = delete;

  ~FakeDesktopMediaPickerFactory() override;

  //  |test_flags| are expected to outlive the factory.
  void SetTestFlags(TestFlags* test_flags, int tests_count);
  FakeDesktopMediaPicker* picker() const { return picker_; }
  bool IsWebContentsExcluded() const { return is_web_contents_excluded_; }
  // DesktopMediaPickerFactory implementation
  std::unique_ptr<DesktopMediaPicker> CreatePicker(
      const content::MediaStreamRequest* request) override;
  std::vector<std::unique_ptr<DesktopMediaList>> CreateMediaList(
      const std::vector<DesktopMediaList::Type>& types,
      content::WebContents* web_contents,
      DesktopMediaList::WebContentsFilter includable_web_contents_filter)
      override;

 private:
  raw_ptr<FakeDesktopMediaPicker, AcrossTasksDanglingUntriaged> picker_;
  raw_ptr<TestFlags, AcrossTasksDanglingUntriaged | AllowPtrArithmetic>
      test_flags_;
  int tests_count_;
  int current_test_;
  bool is_web_contents_excluded_ = false;
};

class FakeDesktopMediaPicker : public DesktopMediaPicker {
 public:
  explicit FakeDesktopMediaPicker(
      FakeDesktopMediaPickerFactory::TestFlags* expectation);

  FakeDesktopMediaPicker(const FakeDesktopMediaPicker&) = delete;
  FakeDesktopMediaPicker& operator=(const FakeDesktopMediaPicker&) = delete;

  ~FakeDesktopMediaPicker() override;

  // DesktopMediaPicker interface.
  void Show(const DesktopMediaPicker::Params& params,
            std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
            DoneCallback done_callback) override;

  DesktopMediaPicker::Params GetParams();

 private:
  void CallCallback(DoneCallback done_callback);

  raw_ptr<FakeDesktopMediaPickerFactory::TestFlags> expectation_;
  DoneCallback done_callback_;
  DesktopMediaPicker::Params picker_params_;

  base::WeakPtrFactory<FakeDesktopMediaPicker> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_FAKE_DESKTOP_MEDIA_PICKER_FACTORY_H_
