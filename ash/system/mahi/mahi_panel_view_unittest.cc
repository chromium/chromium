// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_view.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "chromeos/components/mahi/public/cpp/fake_mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

class MahiPanelViewTest : public AshTestBase {
 public:
  MahiPanelViewTest() = default;
  MahiPanelViewTest(const MahiPanelViewTest&) = delete;
  MahiPanelViewTest& operator=(const MahiPanelViewTest&) = delete;
  ~MahiPanelViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    fake_mahi_manager_ = std::make_unique<chromeos::FakeMahiManager>();
    scoped_setter_ = std::make_unique<chromeos::ScopedMahiManagerSetter>(
        fake_mahi_manager_.get());
    AshTestBase::SetUp();
  }

  void TearDown() override {
    scoped_setter_.reset();
    fake_mahi_manager_.reset();
    AshTestBase::TearDown();
  }

  chromeos::FakeMahiManager* fake_mahi_manager() {
    return fake_mahi_manager_.get();
  }

 private:
  std::unique_ptr<chromeos::FakeMahiManager> fake_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
};

// Makes sure that the summary text is set correctly in ctor with different
// texts.
TEST_F(MahiPanelViewTest, SummaryText) {
  auto* test_text1 = u"test summary text 1";
  fake_mahi_manager()->set_summary_text(test_text1);
  auto mahi_view1 = std::make_unique<MahiPanelView>();
  EXPECT_EQ(test_text1, mahi_view1->summary_label_for_test()->GetText());

  auto* test_text2 = u"test summary text 2";
  fake_mahi_manager()->set_summary_text(test_text2);
  auto mahi_view2 = std::make_unique<MahiPanelView>();
  EXPECT_EQ(test_text2, mahi_view2->summary_label_for_test()->GetText());
}

}  // namespace
}  // namespace ash
