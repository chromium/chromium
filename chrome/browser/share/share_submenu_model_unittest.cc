// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#include "chrome/browser/share/share_submenu_model.h"

namespace share {
namespace {

bool HasItemWithName(const ShareSubmenuModel& model, int string_id) {
  std::u16string name = l10n_util::GetStringUTF16(string_id);
  for (int i = 0; i < model.GetItemCount(); ++i) {
    if (model.GetLabelAt(i) == name)
      return true;
  }
  return false;
}

using ShareSubmenuModelTest = ::testing::Test;

TEST(ShareSubmenuModelTest, CopyItemPresentForLink) {
  ShareSubmenuModel model(nullptr, nullptr, ShareSubmenuModel::Context::LINK,
                          GURL("https://www.chromium.org"));
  EXPECT_TRUE(HasItemWithName(model, IDS_CONTENT_CONTEXT_COPYLINKLOCATION));
}

TEST(ShareSubmenuModelTest, CopyItemPresentForImage) {
  ShareSubmenuModel model(nullptr, nullptr, ShareSubmenuModel::Context::IMAGE,
                          GURL("https://www.chromium.org/image.png"));
  EXPECT_TRUE(HasItemWithName(model, IDS_CONTENT_CONTEXT_COPYIMAGELOCATION));
}

TEST(ShareSubmenuModelTest, CopyItemPresentForEmail) {
  ShareSubmenuModel model(nullptr, nullptr, ShareSubmenuModel::Context::LINK,
                          GURL("mailto:example@chromium.org"));
  EXPECT_TRUE(HasItemWithName(model, IDS_CONTENT_CONTEXT_COPYEMAILADDRESS));
}

}  // namespace
}  // namespace share
