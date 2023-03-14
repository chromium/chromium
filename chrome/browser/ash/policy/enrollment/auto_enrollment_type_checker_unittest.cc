// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/components/system/factory_ping_embargo_check.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kSerialNumberValue[] = "a_value";
constexpr char kBrandCodeValue[] = "brand_code";
constexpr char kActivateDateValue[] = "activated";
constexpr char kMalformedEmbargoDateValue[] = "adventure_time";

std::string ToUTCString(const base::Time& time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);

  const std::string time_string = base::StringPrintf(
      "%04d-%02d-%02d", exploded.year, exploded.month, exploded.day_of_month);

  base::Time reparsed_time;
  EXPECT_TRUE(base::Time::FromUTCString(time_string.c_str(), &reparsed_time));
  base::Time::Exploded reparsed_exploded;
  reparsed_time.UTCExplode(&reparsed_exploded);
  EXPECT_EQ(exploded.year, reparsed_exploded.year);
  EXPECT_EQ(exploded.month, reparsed_exploded.month);
  EXPECT_EQ(exploded.day_of_month, reparsed_exploded.day_of_month);

  return time_string;
}

}  // namespace

namespace policy {

class AutoEnrollmentTypeCheckerTest : public testing::Test {
 public:
  AutoEnrollmentTypeCheckerTest() = default;
  ~AutoEnrollmentTypeCheckerTest() override = default;

 protected:
  void SetupFREEnabled() {
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableForcedReEnrollment,
        AutoEnrollmentTypeChecker::kForcedReEnrollmentAlways);
    command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kEnterpriseEnrollmentInitialModulus);
    command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kEnterpriseEnrollmentModulusLimit);

    ASSERT_TRUE(AutoEnrollmentTypeChecker::IsFREEnabled());
  }

  void SetupFREDisabled() {
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableForcedReEnrollment,
        AutoEnrollmentTypeChecker::kForcedReEnrollmentNever);

    ASSERT_FALSE(AutoEnrollmentTypeChecker::IsFREEnabled());
  }

  void SetupFREEnabledButNotRequired() {
    SetupFREEnabled();

    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
    fake_statistics_provider_.ClearMachineStatistic(
        ash::system::kActivateDateKey);

    ASSERT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kNotRequired);
  }

  void SetupFREEnabledAndRequired() {
    SetupFREEnabled();

    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
    fake_statistics_provider_.SetMachineStatistic(ash::system::kActivateDateKey,
                                                  kActivateDateValue);

    ASSERT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kRequired);
  }

  void SetupInitialEnrollmentEnabled() {
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableInitialEnrollment,
        AutoEnrollmentTypeChecker::kInitialEnrollmentAlways);

    ASSERT_TRUE(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled());
  }

  void SetupInitialEnrollmentEnabledButNotRequired() {
    SetupInitialEnrollmentEnabled();

    fake_statistics_provider_.ClearMachineStatistic(
        ash::system::kSerialNumberKeyForTest);
  }

  void SetupInitialEnrollmentEnabledAndRequired() {
    SetupInitialEnrollmentEnabled();

    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kSerialNumberKeyForTest, kSerialNumberValue);
    fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                  kBrandCodeValue);
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kEnterpriseManagementEmbargoEndDateKey,
        kMalformedEmbargoDateValue);
  }

  base::test::ScopedCommandLine command_line_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

TEST_F(AutoEnrollmentTypeCheckerTest, FREEnabledWhenSwitchIsAlways) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentAlways);

  EXPECT_TRUE(AutoEnrollmentTypeChecker::IsFREEnabled());
}

// Without this macro Chrome is never branded so test always fail. Disable them
// because there there's nothing to test in this case.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_FREEnabledWhenSwitchIsOfficialBuild \
  DISABLED_FREEnabledWhenSwitchIsOfficialBuild
#define MAYBE_FREEnabledWhenSwitchIsEmpty DISABLED_FREEnabledWhenSwitchIsEmpty
#else
#define MAYBE_FREEnabledWhenSwitchIsOfficialBuild \
  FREEnabledWhenSwitchIsOfficialBuild
#define MAYBE_FREEnabledWhenSwitchIsEmpty FREEnabledWhenSwitchIsEmpty
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(AutoEnrollmentTypeCheckerTest,
       MAYBE_FREEnabledWhenSwitchIsOfficialBuild) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNormal);
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentOfficialBuild);

  EXPECT_TRUE(AutoEnrollmentTypeChecker::IsFREEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest, MAYBE_FREEnabledWhenSwitchIsEmpty) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNormal);
  command_line_.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kEnterpriseEnableForcedReEnrollment);

  EXPECT_TRUE(AutoEnrollmentTypeChecker::IsFREEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FREDisabledWhenSwitchIsOfficialBuildButItsNot) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNonchrome);
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentOfficialBuild);

  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsFREEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest, FREDisabledWhenSwitchIsNever) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentNever);

  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsFREEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       InitialEnrollmentEnabledWhenSwitchIsAlways) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment,
      AutoEnrollmentTypeChecker::kInitialEnrollmentAlways);

  EXPECT_TRUE(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled());
}

// Without this macro Chrome is never branded so test always fail. Disable them
// because there there's nothing to test in this case.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_InitialEnrollmentEnabledWhenSwitchIsOfficialBuild \
  DISABLED_InitialEnrollmentEnabledWhenSwitchIsOfficialBuild
#define MAYBE_InitialEnrollmentEnabledWhenSwitchIsEmpty \
  DISABLED_InitialEnrollmentEnabledWhenSwitchIsEmpty
#else
#define MAYBE_InitialEnrollmentEnabledWhenSwitchIsOfficialBuild \
  InitialEnrollmentEnabledWhenSwitchIsOfficialBuild
#define MAYBE_InitialEnrollmentEnabledWhenSwitchIsEmpty \
  InitialEnrollmentEnabledWhenSwitchIsEmpty
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(AutoEnrollmentTypeCheckerTest,
       MAYBE_InitialEnrollmentEnabledWhenSwitchIsOfficialBuild) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNormal);
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kInitialEnrollmentOfficialBuild);

  EXPECT_TRUE(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       MAYBE_InitialEnrollmentEnabledWhenSwitchIsEmpty) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNormal);
  command_line_.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kEnterpriseEnableInitialEnrollment);

  EXPECT_TRUE(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       InitialEnrollmentDisabledWhenSwitchIsOfficialBuildButItsNot) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNonchrome);
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment,
      AutoEnrollmentTypeChecker::kInitialEnrollmentOfficialBuild);

  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       InitialEnrollmentDisabledWhenSwitchIsNever) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment,
      AutoEnrollmentTypeChecker::kInitialEnrollmentNever);

  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FREExplicitlyNotRequiredAccordingToVPDWhenFlagIsZero) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kCheckEnrollmentKey, "0");

  EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                &fake_statistics_provider_),
            AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyNotRequired);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FREExplicitlyRequiredAccordingToVPDWhenFlagIsOne) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kCheckEnrollmentKey, "1");

  EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                &fake_statistics_provider_),
            AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyRequired);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FREExplicitlyRequiredAccordingToVPDWhenFlagIsInvalid) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kCheckEnrollmentKey, "woops");

  EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                &fake_statistics_provider_),
            AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyRequired);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FRENotRequiredAccordingToVPDWhenVPDCanBeReadButDeviceIsNotOwned) {
  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
    // Not setting |kActivateDateKey| statistic to indicate a lack of ownership.

    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kNotRequired);
  }

  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kRoInvalid);
    // Not setting |kActivateDateKey| statistic to indicate a lack of ownership.

    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kNotRequired);
  }

  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kInvalid);
    // Not setting |kActivateDateKey| statistic to indicate a lack of ownership.

    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kRequired);
  }
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FRERequiredAccordingToVPDWhenVPDIsBroken) {
  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kRwInvalid);

    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyRequired);
  }

  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kInvalid);
    command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kRevenBranding);

    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kRequired);
  }
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FRERequiredAccordingToVPDWhenDeviceIsOwned) {
  fake_statistics_provider_.SetMachineStatistic(ash::system::kActivateDateKey,
                                                kActivateDateValue);

  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kRequired);
  }

  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kRoInvalid);

    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kRequired);
  }

  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kInvalid);

    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kRequired);
  }
}

// This is parametrized with dev_disable_boot.
class AutoEnrollmentTypeCheckerTestP
    : public AutoEnrollmentTypeCheckerTest,
      public testing::WithParamInterface<bool> {
 public:
  // Helper function for all situations in which `dev_disable_boot == true` will
  // be interpreted as `kForcedReEnrollmentExplicitlyRequired`.
  AutoEnrollmentTypeChecker::CheckType fre_or(
      AutoEnrollmentTypeChecker::CheckType other) {
    return dev_disable_boot_ ? AutoEnrollmentTypeChecker::CheckType::
                                   kForcedReEnrollmentExplicitlyRequired
                             : other;
  }
  const bool dev_disable_boot_ = GetParam();
};

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckNotRequiredWhenDisabled) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentNever);
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment,
      AutoEnrollmentTypeChecker::kInitialEnrollmentNever);

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
}

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckNotRequiredWhenGaiaServicesDisabled) {
  SetupFREEnabledAndRequired();
  SetupInitialEnrollmentEnabledAndRequired();

  command_line_.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDisableGaiaServices);

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
}

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckNotRequiredWhenFREExplicitlyNotRequired) {
  SetupFREEnabled();
  // Set initial enrollment required. It checks that FRE has priority over
  // initial enrollment.
  SetupInitialEnrollmentEnabledAndRequired();

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kCheckEnrollmentKey, "0");

  EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                &fake_statistics_provider_),
            AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyNotRequired);
  EXPECT_EQ(
      AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
          /*is_system_clock_synchronized=*/true, &fake_statistics_provider_,
          dev_disable_boot_),
      fre_or(AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination));
  EXPECT_EQ(
      AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
          /*is_system_clock_synchronized=*/false, &fake_statistics_provider_,
          dev_disable_boot_),
      fre_or(AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination));
}

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckNotRequiredWhenNoEnrollmentModulusSwitchPresent) {
  SetupFREEnabled();
  SetupInitialEnrollmentEnabledButNotRequired();
  fake_statistics_provider_.SetVpdStatus(
      ash::system::StatisticsProvider::VpdStatus::kValid);

  command_line_.GetProcessCommandLine()->RemoveSwitch(
      ash::switches::kEnterpriseEnrollmentInitialModulus);
  command_line_.GetProcessCommandLine()->RemoveSwitch(
      ash::switches::kEnterpriseEnrollmentModulusLimit);

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
}

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckRequiredWhenFREExplicitlyRequired) {
  SetupFREEnabled();
  // Set initial enrollment required. It checks that FRE has priority over
  // initial enrollment.
  SetupInitialEnrollmentEnabledAndRequired();

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kCheckEnrollmentKey, "1");

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::
                kForcedReEnrollmentExplicitlyRequired);
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::
                kForcedReEnrollmentExplicitlyRequired);
}

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckRequiredWhenFREImplicitlyRequired) {
  SetupFREEnabled();
  // Set initial enrollment required. It checks that FRE has priority over
  // initial enrollment.
  SetupInitialEnrollmentEnabledAndRequired();

  fake_statistics_provider_.SetVpdStatus(
      ash::system::StatisticsProvider::VpdStatus::kValid);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kActivateDateKey,
                                                kActivateDateValue);

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            fre_or(AutoEnrollmentTypeChecker::CheckType::
                       kForcedReEnrollmentImplicitlyRequired));
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            fre_or(AutoEnrollmentTypeChecker::CheckType::
                       kForcedReEnrollmentImplicitlyRequired));
}

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckNotRequiredWhenInitialEnrollmentDisabled) {
  SetupFREEnabledButNotRequired();

  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment,
      AutoEnrollmentTypeChecker::kInitialEnrollmentNever);

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            fre_or(AutoEnrollmentTypeChecker::CheckType::kNone));
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            fre_or(AutoEnrollmentTypeChecker::CheckType::kNone));
}

TEST_P(
    AutoEnrollmentTypeCheckerTestP,
    AutoEnrollmentCheckNotRequiredWhenInitialEnrollmentNotRequiredWhenVPDIsBroken) {
  // FRE turns required when it does not find serial number. Disable it
  // altogether.
  SetupFREDisabled();
  SetupInitialEnrollmentEnabled();

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kSerialNumberKeyForTest, "");

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
}

TEST_P(
    AutoEnrollmentTypeCheckerTestP,
    AutoEnrollmentCheckNotRequiredWhenInitialEnrollmentNotRequiredWhenBrandCodeIsMissing) {
  SetupFREEnabledButNotRequired();
  SetupInitialEnrollmentEnabled();

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kSerialNumberKeyForTest, kSerialNumberValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                "");

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            fre_or(AutoEnrollmentTypeChecker::CheckType::kNone));
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            fre_or(AutoEnrollmentTypeChecker::CheckType::kNone));
}

TEST_P(
    AutoEnrollmentTypeCheckerTestP,
    AutoEnrollmentCheckUnknownWhenSystemClockNotSynchedAndNotRequiredWhenSynched) {
  SetupFREEnabledButNotRequired();
  SetupInitialEnrollmentEnabled();

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kSerialNumberKeyForTest, kSerialNumberValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                kBrandCodeValue);

  {
    // Put embargo embargo day way in the future.
    const auto past_embargo_threshold =
        ToUTCString(base::Time::Now() +
                    2 * ash::system::kEmbargoEndDateGarbageDateThreshold);
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kEnterpriseManagementEmbargoEndDateKey,
        past_embargo_threshold);

    EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                  /*is_system_clock_synchronized=*/false,
                  &fake_statistics_provider_, dev_disable_boot_),
              fre_or(AutoEnrollmentTypeChecker::CheckType::
                         kUnknownDueToMissingSystemClockSync));
    EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                  /*is_system_clock_synchronized=*/true,
                  &fake_statistics_provider_, dev_disable_boot_),
              fre_or(AutoEnrollmentTypeChecker::CheckType::kNone));
  }

  {
    // Put embargo embargo day a little bit in the future.
    const auto before_embargo_threshold =
        ToUTCString(base::Time::Now() +
                    ash::system::kEmbargoEndDateGarbageDateThreshold / 2);
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kEnterpriseManagementEmbargoEndDateKey,
        before_embargo_threshold);

    EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                  /*is_system_clock_synchronized=*/false,
                  &fake_statistics_provider_, dev_disable_boot_),
              fre_or(AutoEnrollmentTypeChecker::CheckType::
                         kUnknownDueToMissingSystemClockSync));
    EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                  /*is_system_clock_synchronized=*/true,
                  &fake_statistics_provider_, dev_disable_boot_),
              fre_or(AutoEnrollmentTypeChecker::CheckType::kNone));
  }
}

TEST_P(
    AutoEnrollmentTypeCheckerTestP,
    AutoEnrollmentCheckRequiredWhenInitialEnrollmentRequiredWhenEmbargoDateMissingOrPassed) {
  SetupFREEnabledButNotRequired();
  SetupInitialEnrollmentEnabled();

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kSerialNumberKeyForTest, kSerialNumberValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                kBrandCodeValue);

  {
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kEnterpriseManagementEmbargoEndDateKey,
        kMalformedEmbargoDateValue);
    EXPECT_EQ(
        AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
            /*is_system_clock_synchronized=*/false, &fake_statistics_provider_,
            dev_disable_boot_),
        fre_or(
            AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination));
    EXPECT_EQ(
        AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
            /*is_system_clock_synchronized=*/true, &fake_statistics_provider_,
            dev_disable_boot_),
        fre_or(
            AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination));
  }

  {
    const auto yeasterday_embargo =
        ToUTCString(base::Time::Now() - base::Days(1));
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kEnterpriseManagementEmbargoEndDateKey,
        yeasterday_embargo);
    EXPECT_EQ(
        AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
            /*is_system_clock_synchronized=*/false, &fake_statistics_provider_,
            dev_disable_boot_),
        fre_or(
            AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination));
    EXPECT_EQ(
        AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
            /*is_system_clock_synchronized=*/true, &fake_statistics_provider_,
            dev_disable_boot_),
        fre_or(
            AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination));
  }
}

INSTANTIATE_TEST_SUITE_P(AutoEnrollmentTypeCheckerTestSuite,
                         AutoEnrollmentTypeCheckerTestP,
                         testing::Bool());

}  // namespace policy
