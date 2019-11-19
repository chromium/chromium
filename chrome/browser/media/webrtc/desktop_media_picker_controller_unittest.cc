// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::ByMove;
using testing::InvokeArgument;
using testing::Ne;
using testing::Return;
using testing::ReturnRef;
using testing::WithArg;

class MockDesktopMediaPicker : public DesktopMediaPicker {
 public:
  MOCK_METHOD3(Show,
               void(const Params& params,
                    std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
                    DoneCallback done_callback));
};

class MockDesktopMediaList : public DesktopMediaList {
 public:
  MOCK_METHOD1(SetUpdatePeriod, void(base::TimeDelta period));
  MOCK_METHOD1(SetThumbnailSize, void(const gfx::Size& thumbnail_size));
  MOCK_METHOD1(SetViewDialogWindowId, void(content::DesktopMediaID dialog_id));
  MOCK_METHOD1(StartUpdating, void(DesktopMediaListObserver* observer));
  MOCK_METHOD1(Update, void(UpdateCallback callback));
  MOCK_CONST_METHOD0(GetSourceCount, int());
  MOCK_CONST_METHOD1(GetSource, Source&(int index));
  MOCK_CONST_METHOD0(GetMediaListType, content::DesktopMediaID::Type());
};

class MockDesktopMediaPickerFactory : public DesktopMediaPickerFactory {
 public:
  MOCK_METHOD0(CreatePicker, std::unique_ptr<DesktopMediaPicker>());
  MOCK_METHOD1(CreateMediaList,
               std::vector<std::unique_ptr<DesktopMediaList>>(
                   const std::vector<content::DesktopMediaID::Type>& types));
};

class DesktopMediaPickerControllerTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(factory_, CreatePicker).WillByDefault([this]() {
      return std::unique_ptr<DesktopMediaPicker>(std::move(picker_));
    });
    ON_CALL(factory_, CreateMediaList).WillByDefault([this](const auto& types) {
      std::vector<std::unique_ptr<DesktopMediaList>> lists;
      lists.push_back(std::move(media_list_));
      return lists;
    });
  }

 protected:
  DesktopMediaPickerController::Params picker_params_;
  base::MockCallback<DesktopMediaPickerController::DoneCallback> done_;
  std::vector<content::DesktopMediaID::Type> source_types_{
      content::DesktopMediaID::TYPE_SCREEN};
  content::DesktopMediaID media_id_{content::DesktopMediaID::TYPE_SCREEN, 42};
  std::unique_ptr<MockDesktopMediaPicker> picker_ =
      std::make_unique<MockDesktopMediaPicker>();
  std::unique_ptr<MockDesktopMediaList> media_list_ =
      std::make_unique<MockDesktopMediaList>();
  MockDesktopMediaPickerFactory factory_;
};

// Test that the picker dialog is shown and the selected media ID is returned.
TEST_F(DesktopMediaPickerControllerTest, ShowPicker) {
  EXPECT_CALL(factory_, CreatePicker());
  EXPECT_CALL(factory_, CreateMediaList(source_types_));
  EXPECT_CALL(done_, Run("", media_id_));
  EXPECT_CALL(*picker_, Show)
      .WillOnce(WithArg<2>([&](DesktopMediaPicker::DoneCallback cb) {
        std::move(cb).Run(media_id_);
      }));
  EXPECT_CALL(*media_list_, Update).Times(0);

  DesktopMediaPickerController controller(&factory_);
  controller.Show(picker_params_, source_types_, done_.Get());
}

// Test that a null result is returned in response to WebContentsDestroyed().
TEST_F(DesktopMediaPickerControllerTest, WebContentsDestroyed) {
  EXPECT_CALL(factory_, CreatePicker());
  EXPECT_CALL(factory_, CreateMediaList(source_types_));
  EXPECT_CALL(done_, Run("", content::DesktopMediaID()));
  EXPECT_CALL(*picker_, Show);

  DesktopMediaPickerController controller(&factory_);
  controller.Show(picker_params_, source_types_, done_.Get());
  controller.WebContentsDestroyed();
}

// Test that the picker dialog can be bypassed.
TEST_F(DesktopMediaPickerControllerTest, ShowSingleScreen) {
  picker_params_.select_only_screen = true;

  DesktopMediaList::Source source;
  source.id = media_id_;
  source.name = base::ASCIIToUTF16("fake name");

  EXPECT_CALL(factory_, CreatePicker()).Times(0);
  EXPECT_CALL(factory_, CreateMediaList(source_types_));
  EXPECT_CALL(done_, Run("", source.id));
  EXPECT_CALL(*picker_, Show).Times(0);
  EXPECT_CALL(*media_list_, Update)
      .WillOnce(
          [](DesktopMediaList::UpdateCallback cb) { std::move(cb).Run(); });
  EXPECT_CALL(*media_list_, GetSourceCount)
      .Times(AnyNumber())
      .WillRepeatedly(Return(1));
  EXPECT_CALL(*media_list_, GetSource(0))
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(source));

  DesktopMediaPickerController controller(&factory_);
  controller.Show(picker_params_, source_types_, done_.Get());
}

// Test that an error is reported when no sources are found.
TEST_F(DesktopMediaPickerControllerTest, EmptySourceList) {
  EXPECT_CALL(factory_, CreateMediaList)
      .WillOnce(
          Return(ByMove(std::vector<std::unique_ptr<DesktopMediaList>>())));
  EXPECT_CALL(done_, Run(Ne(""), content::DesktopMediaID()));

  DesktopMediaPickerController controller(&factory_);
  controller.Show(picker_params_, source_types_, done_.Get());
}

// Test that an error is reported when no picker can be created.
TEST_F(DesktopMediaPickerControllerTest, NoPicker) {
  EXPECT_CALL(factory_, CreatePicker)
      .WillOnce(Return(ByMove(std::unique_ptr<DesktopMediaPicker>())));
  EXPECT_CALL(done_, Run(Ne(""), content::DesktopMediaID()));
  EXPECT_CALL(factory_, CreateMediaList).Times(AnyNumber());

  DesktopMediaPickerController controller(&factory_);
  controller.Show(picker_params_, source_types_, done_.Get());
}
