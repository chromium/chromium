// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_UI_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_UI_BROWSER_TEST_BASE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/mahi/mahi_manager_impl.h"
#include "chrome/browser/ash/mahi/web_contents/test_support/fake_mahi_web_contents_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/test/event_generator.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class MahiUiBrowserTestBase : public SystemWebAppBrowserTestBase {
 public:
  explicit MahiUiBrowserTestBase(bool apply_restrictions_override = true);
  MahiUiBrowserTestBase(const MahiUiBrowserTestBase&) = delete;
  MahiUiBrowserTestBase& operator=(const MahiUiBrowserTestBase&) = delete;
  ~MahiUiBrowserTestBase() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

 protected:
  // SystemWebAppBrowserTestBase:
  void SetUpOnMainThread() override;

  // Sets the primary account's `can_use_manta_service` capability, or leaves it
  // in an unknown (`signin::Tribool::kUnknown`) state if `can_use` is
  // `std::nullopt`.
  void SetCanUseMantaService(std::optional<bool> can_use);

  // Mouse clicks on the disclaimer view's accept button or the declination
  // button, specified by `accept`.
  void ClickDisclaimerViewButton(bool accept);

  void TypeStringToMahiMenuTextfield(views::Widget* mahi_menu_widget,
                                     const std::u16string& input);

  void WaitForSettingsToLoad();

  ui::test::EventGenerator& event_generator() { return *event_generator_; }

 private:
  const bool apply_restrictions_override_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  mahi::FakeMahiWebContentsManager fake_mahi_web_contents_manager_;
  chromeos::ScopedMahiWebContentsManagerOverride
      scoped_mahi_web_contents_manager_{&fake_mahi_web_contents_manager_};
  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_UI_BROWSER_TEST_BASE_H_
