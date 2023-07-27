// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_color_manager.h"

#include <memory>

#include "ash/constants/ash_paths.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/quirks/quirks_manager.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/test_native_display_delegate.h"

namespace ash {

namespace {

constexpr gfx::Size kDisplaySize(1024, 768);
const char kResetGammaAction[] = "*set_gamma_correction(id=123)";
const char kSetGammaAction[] =
    "*set_gamma_correction(id=123,gamma[0]*gamma[255]=???????????\?)";
const char kSetFullCTMAction[] =
    "set_color_matrix(id=123,ctm[0]*ctm[8]*),"
    "set_gamma_correction(id=123,degamma[0]*gamma[0]*)";
const char kValidIccProfile[] =
    "H4sIAEY3QWAAA+3SeVjMeRwH8E/TdthOCUkxOuTozrifpDFTVI4KUyL5Nc1GTWOOCiFnzhii"
    "HBEqdxY5CzkXOSJXTEYkiWST5Ghnv9/5an/Tts/uP/vf7vv3zPN7z/f7zDzf3+/zAtBfy0Rh"
    "mALECqXiQB9vJi8klKmnBF2wBB3oCHYRlEQUEMQNBpQgDpsJbdL4ALTwvdTpr/f/NoaRfAkF"
    "oGWOejIlEktRT0PdIUYWG416IermCVIRXi/BXYwOiHol7gLSm3Cfpu4MQ9ypnyIiUcdncRQH"
    "B7JR90fdWEB6GO7TSI/BPZ4SoP9nJAHompHzkLPxOCxXV7abu8Z5jYAHHGCBK7rY4Aaae/9K"
    "pPxEKb4HRcRKZEIBkxPDp6TiOGE0JWGy45wdmf7SSGe0j+ek/oGC/vyxhmcCKhWaiRe9JpEB"
    "sIsBTNLptfB8gAwrAGtbes02D8DOAiA3RRLlQZ5Py9ATQKdcpfrgAqAnB2hOVam+ZqtUzTkA"
    "2mUARZmUTBz//RG0GB4A//SdvPPv0UaAGOotFAaKtjo/qKOjjq6uHoq+frt2P6IYGBgaGhkZ"
    "G5uYmJq2b29m1qGDuXlHlE6dOne2sOjSxdKya1crK2vrbt26d2cye/SwsbG1tbfv2dPBoVev"
    "3r379Onb19HRycnFxdXVzc3d3cOjXz8Wa8CAgQMHDRo8eMiQoUM9PYcN8/IaPtzbe8QIDofL"
    "9fHx9R01ys/P3z8gYPTosWPHjQsMDAoKDp4wYeJEHi8kJDR00qTJk6dMCQ+fOjUigqIiI/n8"
    "qCiBIDp6+vQZM2JiYmOFQpFo5kyxWCKRSuPjExISE2fNmj07KWnu3Hnz5s9PTl6wYNGixYuX"
    "LFm6NCVl2bLly1esWLly1arVq1NT16xZiyKXr1+flrZhw8aN6ekZGZs2bd68dWtm5rZtWVk7"
    "duzcuWtXdnZu7u7de/bs3btv3/79Bw4cPJiXd+jQ4cNHjhw9mp9/7Njx4ydOnDx56tTp0wUF"
    "hYVnzpw9e+5cUdH58xcuXLx46dLly1eu/IJy9eq1a9evFxffuHHz5q1bt2+XlNy5c/duaem9"
    "e/fvP0B5+PDRo8ePnzxRKMrLnz5VKp89q6h4/vzFi8rKly+rUF69qq5+/bqm5s2bt29ra9+9"
    "q6t7//5XderrP6A0NHz82Nj46VNT0+fPX758Rfn2rbn5NxSs9f/5/9fnr2mgRUFrC7rqYAnE"
    "goEBbYFooD20aGjtoUWEjY2dnb09UUG7IDKcnbENrIPYYLH699fUQftgs4kQLtfXd+RIbMTP"
    "DxsZM4ZWMn48dhISQpyEhWEnf5bS2kpcHK1FJiNa5szR1LJwIdHS1opcvm5dixUiZcsWYmX7"
    "9qwsYiUnh2jRtPIzSlsvBQW0FyKG9kK00F40xWAvWEtZGdaiUGAtSmVFBbZSVVVdXVODhdTW"
    "1tVhG/X1DQ0AvwPCW+4LkAgAAA==";
const char kInvalidIccProfile[] =
    "H4sIAEY3QWAAA2NgYFyTk5xbzGLAwFBckJgc5O6kEBEZpcB+n4GbQZyBi4GZQT4xubjAMSDA"
    "hwEn+HaNgRFEX9YFmYVbHVbAUQS0EEg/AWKRdAj7B4idBGYz8oDYRSFBzkC2ApAtkA5hG4DY"
    "SRC2A4hdXlJQAmQHgNSnpBYnA9kpQLYHyD8oNgLdiiLGCKHgYowIpQWJRYnI6gjxEeYiwiQ3"
    "pzQZySae1LzQYCCtA8QyDCEM/gwuQGzFoMDgyJAChApAnMpQzJDMUMSQyVDAUAIk8xnyGBgA"
    "x6c/q6wBAAA=";

class DisplayColorManagerForTest : public DisplayColorManager {
 public:
  explicit DisplayColorManagerForTest(
      display::DisplayConfigurator* configurator)
      : DisplayColorManager(configurator) {}

  DisplayColorManagerForTest(const DisplayColorManagerForTest&) = delete;
  DisplayColorManagerForTest& operator=(const DisplayColorManagerForTest&) =
      delete;

  void SetOnFinishedForTest(base::OnceClosure on_finished_for_test) {
    on_finished_for_test_ = std::move(on_finished_for_test);
  }

 private:
  void FinishLoadCalibrationForDisplay(int64_t display_id,
                                       int64_t product_id,
                                       bool has_color_correction_matrix,
                                       display::DisplayConnectionType type,
                                       const base::FilePath& path,
                                       bool file_downloaded) override {
    DisplayColorManager::FinishLoadCalibrationForDisplay(
        display_id, product_id, has_color_correction_matrix, type, path,
        file_downloaded);
    // If path is empty, there is no icc file, and the DCM's work is done.
    if (path.empty() && on_finished_for_test_) {
      std::move(on_finished_for_test_).Run();
      on_finished_for_test_.Reset();
    }
  }

  void UpdateCalibrationData(
      int64_t display_id,
      int64_t product_id,
      std::unique_ptr<ColorCalibrationData> data) override {
    DisplayColorManager::UpdateCalibrationData(display_id, product_id,
                                               std::move(data));
    if (on_finished_for_test_) {
      std::move(on_finished_for_test_).Run();
      on_finished_for_test_.Reset();
    }
  }

  base::OnceClosure on_finished_for_test_;
};

// Implementation of QuirksManager::Delegate to fake chrome-restricted parts.
class QuirksManagerDelegateTestImpl : public quirks::QuirksManager::Delegate {
 public:
  QuirksManagerDelegateTestImpl(base::FilePath color_path)
      : color_path_(color_path) {}

  QuirksManagerDelegateTestImpl(const QuirksManagerDelegateTestImpl&) = delete;
  QuirksManagerDelegateTestImpl& operator=(
      const QuirksManagerDelegateTestImpl&) = delete;

  // Unused by these tests.
  std::string GetApiKey() const override { return std::string(); }

  base::FilePath GetDisplayProfileDirectory() const override {
    return color_path_;
  }

  bool DevicePolicyEnabled() const override { return true; }

 private:
  ~QuirksManagerDelegateTestImpl() override = default;

  base::FilePath color_path_;
};

}  // namespace

class DisplayColorManagerTest : public testing::Test {
 public:
  void SetUp() override {
    log_ = std::make_unique<display::test::ActionLogger>();

    native_display_delegate_ =
        new display::test::TestNativeDisplayDelegate(log_.get());
    configurator_.SetDelegateForTesting(
        std::unique_ptr<display::NativeDisplayDelegate>(
            native_display_delegate_));

    color_manager_ =
        std::make_unique<DisplayColorManagerForTest>(&configurator_);

    EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &color_path_));

    color_path_ = color_path_.Append(FILE_PATH_LITERAL("ash"))
                      .Append(FILE_PATH_LITERAL("display"))
                      .Append(FILE_PATH_LITERAL("test_data"));
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        DIR_DEVICE_DISPLAY_PROFILES, color_path_);

    quirks::QuirksManager::Initialize(
        std::unique_ptr<quirks::QuirksManager::Delegate>(
            new QuirksManagerDelegateTestImpl(color_path_)),
        nullptr, nullptr);

    system::StatisticsProvider::SetTestProvider(&fake_statistics_provider_);
  }

  void TearDown() override {
    quirks::QuirksManager::Shutdown();
  }

  void WaitOnColorCalibration() {
    base::RunLoop run_loop;
    color_manager_->SetOnFinishedForTest(run_loop.QuitClosure());
    run_loop.Run();
  }

  DisplayColorManagerTest() : test_api_(&configurator_) {}

  DisplayColorManagerTest(const DisplayColorManagerTest&) = delete;
  DisplayColorManagerTest& operator=(const DisplayColorManagerTest&) = delete;

  ~DisplayColorManagerTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  base::FilePath color_path_;
  std::unique_ptr<display::test::ActionLogger> log_;
  display::DisplayConfigurator configurator_;
  display::DisplayConfigurator::TestApi test_api_;
  raw_ptr<display::test::TestNativeDisplayDelegate, ExperimentalAsh>
      native_display_delegate_;  // not owned
  std::unique_ptr<DisplayColorManagerForTest> color_manager_;
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

TEST_F(DisplayColorManagerTest, VCGTOnly) {
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(0x06af5c10)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  // Clear initial configuration log.
  log_->GetActionsAndClear();

  WaitOnColorCalibration();
  EXPECT_TRUE(base::MatchPattern(log_->GetActionsAndClear(), kSetGammaAction));
}

TEST_F(DisplayColorManagerTest, VCGTOnlyWithPlatformCTM) {
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(true)
          .SetProductCode(0x06af5c10)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  log_->GetActionsAndClear();
  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  // Clear initial configuration log.
  log_->GetActionsAndClear();

  WaitOnColorCalibration();
  EXPECT_TRUE(base::MatchPattern(log_->GetActionsAndClear(), kSetGammaAction));
}

TEST_F(DisplayColorManagerTest, FullWithPlatformCTM) {
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(true)
          .SetProductCode(0x4c834a42)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  // Clear initial configuration log.
  log_->GetActionsAndClear();

  WaitOnColorCalibration();
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kSetFullCTMAction));
}

TEST_F(DisplayColorManagerTest, SetDisplayColorMatrixNoCTMSupport) {
  constexpr int64_t kDisplayId = 123;
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(kDisplayId)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(0x4c834a42)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  WaitOnColorCalibration();
  // DisplayColorManager::ResetDisplayColorCalibration() will be called since
  // this display has no CTM support.
  const std::string& actions = log_->GetActionsAndClear();
  EXPECT_TRUE(base::MatchPattern(actions, kResetGammaAction));
  // Hardware doesn't support CTM, so CTM shouldn't be configured.
  EXPECT_FALSE(base::MatchPattern(actions, "*set_color_matrix*"));

  // Attempt to set a color matrix.
  SkM44 matrix;
  matrix.setRC(1, 1, 0.7);
  matrix.setRC(2, 2, 0.3);
  EXPECT_FALSE(color_manager_->SetDisplayColorMatrix(kDisplayId, matrix));
  EXPECT_EQ(color_manager_->displays_ctm_support(),
            DisplayColorManager::DisplayCtmSupport::kNone);
  EXPECT_STREQ("", log_->GetActionsAndClear().c_str());
}

TEST_F(DisplayColorManagerTest,
       SetDisplayColorMatrixWithCTMSupportNoCalibration) {
  constexpr int64_t kDisplayId = 123;
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(kDisplayId)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(true)
          .SetProductCode(0x0)  // Non-existent product code.
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());

  // No need to wait for calibration here, this display doesn't have an icc
  // file.
  log_->GetActionsAndClear();

  // Attempt to set a color matrix.
  SkM44 matrix;
  matrix.setRC(1, 1, 0.7);
  matrix.setRC(2, 2, 0.3);
  EXPECT_TRUE(color_manager_->SetDisplayColorMatrix(kDisplayId, matrix));
  EXPECT_EQ(color_manager_->displays_ctm_support(),
            DisplayColorManager::DisplayCtmSupport::kAll);
  // This display has no color calibration data. Gamma/degamma won't be
  // affected. Color matrix is applied as is.
  EXPECT_TRUE(base::MatchPattern(
      log_->GetActionsAndClear(),
      "set_color_matrix(id=123,ctm[0]=1*ctm[4]=0.7*ctm[8]=0.3*)"));

  // Reconfiguring with the same displays snapshots will reapply the matrix.
  native_display_delegate_->set_outputs(outputs);
  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_TRUE(base::MatchPattern(
      log_->GetActionsAndClear(),
      "*set_color_matrix(id=123,ctm[0]=1*ctm[4]=0.7*ctm[8]=0.3*)"));
}

TEST_F(DisplayColorManagerTest, SetDisplayColorMatrixWithMixedCTMSupport) {
  constexpr int64_t kDisplayWithCtmId = 123;
  std::unique_ptr<display::DisplaySnapshot> snapshot1 =
      display::FakeDisplaySnapshot::Builder()
          .SetId(kDisplayWithCtmId)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(true)
          .SetProductCode(0x0)  // Non-existent product code.
          .Build();
  constexpr int64_t kDisplayNoCtmId = 456;
  std::unique_ptr<display::DisplaySnapshot> snapshot2 =
      display::FakeDisplaySnapshot::Builder()
          .SetId(kDisplayNoCtmId)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_HDMI)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(0x0)  // Non-existent product code.
          .Build();

  std::vector<display::DisplaySnapshot*> outputs(
      {snapshot1.get(), snapshot2.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());

  // No need to wait for calibration here, these displays don't have icc files.
  log_->GetActionsAndClear();

  EXPECT_EQ(color_manager_->displays_ctm_support(),
            DisplayColorManager::DisplayCtmSupport::kMixed);

  // Attempt to set a color matrix.
  SkM44 matrix;
  matrix.setRC(1, 1, 0.7);
  matrix.setRC(2, 2, 0.3);
  EXPECT_TRUE(color_manager_->SetDisplayColorMatrix(kDisplayWithCtmId, matrix));
  // This display has no color calibration data. Gamma/degamma won't be
  // affected. Color matrix is applied as is.
  EXPECT_TRUE(base::MatchPattern(
      log_->GetActionsAndClear(),
      "set_color_matrix(id=123,ctm[0]=1*ctm[4]=0.7*ctm[8]=0.3*)"));

  // No matrix will be applied to this display.
  EXPECT_FALSE(color_manager_->SetDisplayColorMatrix(kDisplayNoCtmId, matrix));
  EXPECT_STREQ("", log_->GetActionsAndClear().c_str());
}

TEST_F(DisplayColorManagerTest,
       SetDisplayColorMatrixWithCTMSupportWithCalibration) {
  constexpr int64_t kDisplayId = 123;
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(kDisplayId)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(true)
          .SetProductCode(0x4c834a42)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());

  WaitOnColorCalibration();
  log_->GetActionsAndClear();

  // Attempt to set a color matrix.
  SkM44 matrix;
  matrix.setRC(1, 1, 0.7);
  matrix.setRC(2, 2, 0.3);
  EXPECT_TRUE(color_manager_->SetDisplayColorMatrix(kDisplayId, matrix));
  EXPECT_EQ(color_manager_->displays_ctm_support(),
            DisplayColorManager::DisplayCtmSupport::kAll);
  // The applied matrix is the combination of this color matrix and the color
  // calibration matrix. Gamma/degamma won't be affected.
  EXPECT_TRUE(base::MatchPattern(
      log_->GetActionsAndClear(),
      "set_color_matrix(id=123,ctm[0]=0.01*ctm[4]=0.5*ctm[8]=0.04*)"));

  // Reconfiguring with the same displays snapshots will reapply the same
  // product matrix as well as gamma/degamma from the calibration data.
  native_display_delegate_->set_outputs(outputs);
  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_TRUE(base::MatchPattern(
      log_->GetActionsAndClear(),
      "*set_color_matrix(id=123,ctm[0]=0.01*ctm[4]=0.5*ctm[8]=0.04*),"
      "set_gamma_correction(id=123,degamma[0]*gamma[0]*)"));
}

TEST_F(DisplayColorManagerTest, FullWithoutPlatformCTM) {
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(0x4c834a42)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  // Clear initial configuration log.
  log_->GetActionsAndClear();

  WaitOnColorCalibration();
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));
}

TEST_F(DisplayColorManagerTest, NoMatchProductID) {
  std::string encoded_icc_profile = "00000000:";
  encoded_icc_profile += kValidIccProfile;
  fake_statistics_provider_.SetMachineStatistic(system::kDisplayProfilesKey,
                                                encoded_icc_profile);
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(0)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  // DisplayColorManager::ResetDisplayColorCalibration() will be called since
  // the product code is invalid.
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));

  // NOTE: If product_code == 0, there is no thread switching in Quirks or
  // Display code, so we shouldn't call WaitOnColorCalibration().
  EXPECT_STREQ("", log_->GetActionsAndClear().c_str());
}

TEST_F(DisplayColorManagerTest, NoVCGT) {
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(0x0dae3211)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  // Clear initial configuration log.
  log_->GetActionsAndClear();

  WaitOnColorCalibration();
  // DisplayColorManager::ResetDisplayColorCalibration() will be called since
  // there is no vcgt table.
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));
}

TEST_F(DisplayColorManagerTest, NoVpdDisplayProfilesEntry) {
  // Set the VPD-written ICC data of |product_id| to be the contents in
  // |icc_path|.
  int64_t product_id = 0x0;  // No matching product ID, so no Quirks ICC.
  const base::FilePath& icc_path = color_path_.Append("06af5c10.icc");
  auto vpd_dir_override = std::make_unique<base::ScopedPathOverride>(
      DIR_DEVICE_DISPLAY_PROFILES_VPD);
  base::FilePath vpd_dir;
  EXPECT_TRUE(
      base::PathService::Get(DIR_DEVICE_DISPLAY_PROFILES_VPD, &vpd_dir));
  EXPECT_TRUE(base::CopyFile(icc_path,
                             vpd_dir.Append(quirks::IdToFileName(product_id))));

  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(product_id)
          .Build();

  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());

  // DisplayColorManager::ResetDisplayColorCalibration() will be called since
  // VPD has no display_profile key:value pair.
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));

  // NOTE: If product_code == 0, there is no thread switching in Quirks or
  // Display code, so we shouldn't call WaitOnColorCalibration().
  EXPECT_STREQ("", log_->GetActionsAndClear().c_str());
}

TEST_F(DisplayColorManagerTest, VpdCalibration) {
  // Set the VPD-written ICC data of |product_id| to be the contents in
  // |icc_path|.
  int64_t product_id = 0x0;  // No matching product ID, so no Quirks ICC.
  std::string encoded_icc_profile = "00000000:";
  encoded_icc_profile += kValidIccProfile;
  fake_statistics_provider_.SetMachineStatistic(system::kDisplayProfilesKey,
                                                encoded_icc_profile);
  const base::FilePath& icc_path = color_path_.Append("06af5c10.icc");
  auto vpd_dir_override = std::make_unique<base::ScopedPathOverride>(
      DIR_DEVICE_DISPLAY_PROFILES_VPD);
  base::FilePath vpd_dir;
  EXPECT_TRUE(
      base::PathService::Get(DIR_DEVICE_DISPLAY_PROFILES_VPD, &vpd_dir));
  EXPECT_TRUE(base::CopyFile(icc_path,
                             vpd_dir.Append(quirks::IdToFileName(product_id))));

  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(product_id)
          .Build();

  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  // Clear initial configuration log.
  log_->GetActionsAndClear();

  WaitOnColorCalibration();
  // There is no calibration for this product in Quirks, so confirm that the
  // VPD-written data is applied.
  EXPECT_TRUE(base::MatchPattern(log_->GetActionsAndClear(), kSetGammaAction));
}

TEST_F(DisplayColorManagerTest, QuirksCalibration) {
  // Set the VPD-written ICC data of |product_id| to be the contents in
  // |icc_path|.
  int64_t product_id = 0x06af5c10;
  std::string encoded_icc_profile = "06af5c10:";
  encoded_icc_profile += kInvalidIccProfile;
  fake_statistics_provider_.SetMachineStatistic(system::kDisplayProfilesKey,
                                                encoded_icc_profile);
  const base::FilePath& icc_path = color_path_.Append("4c834a42.icc");
  auto quirks_dir_override = std::make_unique<base::ScopedPathOverride>(
      DIR_DEVICE_DISPLAY_PROFILES_VPD);
  base::FilePath quirks_dir;
  EXPECT_TRUE(
      base::PathService::Get(DIR_DEVICE_DISPLAY_PROFILES_VPD, &quirks_dir));
  EXPECT_TRUE(base::CopyFile(
      icc_path, quirks_dir.Append(quirks::IdToFileName(product_id))));
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(product_id)
          .Build();

  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  // Clear initial configuration log.
  log_->GetActionsAndClear();

  // The VPD-written ICC has no vcgt table and would call
  // DisplayColorManager::ResetDisplayColorCalibration().
  WaitOnColorCalibration();
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));
  // Confirm that the Quirks-fetched ICC, which does, is what is applied.
  WaitOnColorCalibration();
  EXPECT_TRUE(base::MatchPattern(log_->GetActionsAndClear(), kSetGammaAction));
}

TEST_F(DisplayColorManagerTest, VpdCalibrationWithQuirks) {
  // Set the VPD-written ICC data of |product_id| to be the contents in
  // |icc_path|.
  int64_t product_id = 0x06af5c10;
  std::string encoded_icc_profile = "06af5c10:";
  encoded_icc_profile += kValidIccProfile;
  fake_statistics_provider_.SetMachineStatistic(system::kDisplayProfilesKey,
                                                encoded_icc_profile);
  const base::FilePath& icc_path = color_path_.Append("06af5c10.icc");
  auto vpd_dir_override = std::make_unique<base::ScopedPathOverride>(
      DIR_DEVICE_DISPLAY_PROFILES_VPD);
  base::FilePath vpd_dir;
  EXPECT_TRUE(
      base::PathService::Get(DIR_DEVICE_DISPLAY_PROFILES_VPD, &vpd_dir));
  EXPECT_TRUE(base::CopyFile(icc_path,
                             vpd_dir.Append(quirks::IdToFileName(product_id))));

  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(product_id)
          .Build();

  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  // Clear initial configuration log.
  log_->GetActionsAndClear();

  // Both the Quirks-fetched ICC and the the VPD-written ICC are valid.
  WaitOnColorCalibration();
  EXPECT_TRUE(base::MatchPattern(log_->GetActionsAndClear(), kSetGammaAction));
}

}  // namespace ash
