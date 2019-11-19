// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_list.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeDesktopMediaPicker : public DesktopMediaPicker {
 public:
  explicit FakeDesktopMediaPicker(
      FakeDesktopMediaPickerFactory::TestFlags* expectation)
      : expectation_(expectation) {
    expectation_->picker_created = true;
  }
  ~FakeDesktopMediaPicker() override { expectation_->picker_deleted = true; }

  // DesktopMediaPicker interface.
  void Show(const DesktopMediaPicker::Params& params,
            std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
            DoneCallback done_callback) override {
    bool show_screens = false;
    bool show_windows = false;
    bool show_tabs = false;

    for (auto& source_list : source_lists) {
      switch (source_list->GetMediaListType()) {
        case content::DesktopMediaID::TYPE_NONE:
          break;
        case content::DesktopMediaID::TYPE_SCREEN:
          show_screens = true;
          break;
        case content::DesktopMediaID::TYPE_WINDOW:
          show_windows = true;
          break;
        case content::DesktopMediaID::TYPE_WEB_CONTENTS:
          show_tabs = true;
          break;
      }
    }
    EXPECT_EQ(expectation_->expect_screens, show_screens);
    EXPECT_EQ(expectation_->expect_windows, show_windows);
    EXPECT_EQ(expectation_->expect_tabs, show_tabs);
    EXPECT_EQ(expectation_->expect_audio, params.request_audio);
    EXPECT_EQ(params.modality, ui::ModalType::MODAL_TYPE_CHILD);

    if (!expectation_->cancelled) {
      // Post a task to call the callback asynchronously.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&FakeDesktopMediaPicker::CallCallback,
                         weak_factory_.GetWeakPtr(), std::move(done_callback)));
    } else {
      // If we expect the dialog to be cancelled then store the callback to
      // retain reference to the callback handler.
      done_callback_ = std::move(done_callback);
    }
  }

 private:
  void CallCallback(DoneCallback done_callback) {
    std::move(done_callback).Run(expectation_->selected_source);
  }

  FakeDesktopMediaPickerFactory::TestFlags* expectation_;
  DoneCallback done_callback_;

  base::WeakPtrFactory<FakeDesktopMediaPicker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopMediaPicker);
};

FakeDesktopMediaPickerFactory::FakeDesktopMediaPickerFactory() = default;

FakeDesktopMediaPickerFactory::~FakeDesktopMediaPickerFactory() = default;

void FakeDesktopMediaPickerFactory::SetTestFlags(TestFlags* test_flags,
                                                 int tests_count) {
  test_flags_ = test_flags;
  tests_count_ = tests_count;
  current_test_ = 0;
}

std::unique_ptr<DesktopMediaPicker>
FakeDesktopMediaPickerFactory::CreatePicker() {
  EXPECT_LE(current_test_, tests_count_);
  if (current_test_ >= tests_count_)
    return std::unique_ptr<DesktopMediaPicker>();
  ++current_test_;
  return std::unique_ptr<DesktopMediaPicker>(
      new FakeDesktopMediaPicker(test_flags_ + current_test_ - 1));
}

std::vector<std::unique_ptr<DesktopMediaList>>
FakeDesktopMediaPickerFactory::CreateMediaList(
    const std::vector<content::DesktopMediaID::Type>& types) {
  EXPECT_LE(current_test_, tests_count_);
  std::vector<std::unique_ptr<DesktopMediaList>> media_lists;
  for (auto source_type : types)
    media_lists.emplace_back(new FakeDesktopMediaList(source_type));
  return media_lists;
}
