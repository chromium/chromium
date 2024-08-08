// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"

FakeDesktopMediaPicker::FakeDesktopMediaPicker(
    FakeDesktopMediaPickerFactory::TestFlags* expectation)
    : expectation_(expectation),
      picker_params_(Params::RequestSource::kUnknown) {
  expectation_->picker_created = true;
}
FakeDesktopMediaPicker::~FakeDesktopMediaPicker() {
  expectation_->picker_deleted = true;
}

// DesktopMediaPicker interface.
void FakeDesktopMediaPicker::Show(
    const DesktopMediaPicker::Params& params,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
    DoneCallback done_callback) {
  bool show_screens = false;
  bool show_windows = false;
  bool show_tabs = false;
  bool show_current_tab = false;
  picker_params_ = params;

  for (auto& source_list : source_lists) {
    switch (source_list->GetMediaListType()) {
      case DesktopMediaList::Type::kNone:
        break;
      case DesktopMediaList::Type::kScreen:
        show_screens = true;
        break;
      case DesktopMediaList::Type::kWindow:
        show_windows = true;
        break;
      case DesktopMediaList::Type::kWebContents:
        show_tabs = true;
        break;
      case DesktopMediaList::Type::kCurrentTab:
        show_current_tab = true;
        break;
    }
  }
  EXPECT_EQ(expectation_->expect_screens, show_screens);
  EXPECT_EQ(expectation_->expect_windows, show_windows);
  EXPECT_EQ(expectation_->expect_tabs, show_tabs);
  EXPECT_EQ(expectation_->expect_current_tab, show_current_tab);
  EXPECT_EQ(expectation_->expect_audio, params.request_audio);
  EXPECT_EQ(params.modality, ui::mojom::ModalType::kChild);

  if (!expectation_->cancelled) {
    // Post a task to call the callback asynchronously.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeDesktopMediaPicker::CallCallback,
                       weak_factory_.GetWeakPtr(), std::move(done_callback)));
  } else {
    // If we expect the dialog to be cancelled then store the callback to
    // retain reference to the callback handler.
    done_callback_ = std::move(done_callback);
  }
}

DesktopMediaPicker::Params FakeDesktopMediaPicker::GetParams() {
  return picker_params_;
}

void FakeDesktopMediaPicker::CallCallback(DoneCallback done_callback) {
  std::move(done_callback).Run(expectation_->selected_source);
}

FakeDesktopMediaPickerFactory::FakeDesktopMediaPickerFactory() = default;

FakeDesktopMediaPickerFactory::~FakeDesktopMediaPickerFactory() = default;

void FakeDesktopMediaPickerFactory::SetTestFlags(TestFlags* test_flags,
                                                 int tests_count) {
  test_flags_ = test_flags;
  tests_count_ = tests_count;
  current_test_ = 0;
}

std::unique_ptr<DesktopMediaPicker> FakeDesktopMediaPickerFactory::CreatePicker(
    const content::MediaStreamRequest* request) {
  EXPECT_LE(current_test_, tests_count_);
  if (current_test_ >= tests_count_)
    return nullptr;
  ++current_test_;
  picker_ = new FakeDesktopMediaPicker(test_flags_ + current_test_ - 1);
  return std::unique_ptr<DesktopMediaPicker>(picker_);
}

std::vector<std::unique_ptr<DesktopMediaList>>
FakeDesktopMediaPickerFactory::CreateMediaList(
    const std::vector<DesktopMediaList::Type>& types,
    content::WebContents* web_contents,
    DesktopMediaList::WebContentsFilter includable_web_contents_filter) {
  EXPECT_LE(current_test_, tests_count_);
  is_web_contents_excluded_ = !includable_web_contents_filter.Run(web_contents);
  std::vector<std::unique_ptr<DesktopMediaList>> media_lists;
  for (auto source_type : types)
    media_lists.emplace_back(new FakeDesktopMediaList(source_type));
  return media_lists;
}
