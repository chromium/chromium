// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/calculators_policies_binder.h"

#include <list>
#include <map>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/printing/bulk_printers_calculator.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

namespace chromeos {

namespace {

// It stores the number of bindings (instances of this class) connected to each
// BulkPrintersCalculator object. It allows us to make sure, that every
// BulkPrintersCalculator object is not binded more that once.
std::map<BulkPrintersCalculator*, unsigned>& BindingsCount() {
  static base::NoDestructor<std::map<BulkPrintersCalculator*, unsigned>>
      bindings_count;
  return *bindings_count;
}

BulkPrintersCalculator::AccessMode ConvertToAccessMode(int mode_val) {
  if (mode_val >= BulkPrintersCalculator::BLACKLIST_ONLY &&
      mode_val <= BulkPrintersCalculator::ALL_ACCESS) {
    return static_cast<BulkPrintersCalculator::AccessMode>(mode_val);
  }
  // Error occurred, let's return the default value.
  LOG(ERROR) << "Unrecognized access mode";
  return BulkPrintersCalculator::ALL_ACCESS;
}

std::vector<std::string> ConvertToVector(const base::ListValue* list) {
  std::vector<std::string> string_list;
  if (list) {
    for (const base::Value& value : *list) {
      if (value.is_string()) {
        string_list.push_back(value.GetString());
      }
    }
  }
  return string_list;
}

class CalculatorsPoliciesBinderImpl : public CalculatorsPoliciesBinder {
 public:
  CalculatorsPoliciesBinderImpl(CrosSettings* settings, Profile* profile)
      : settings_(settings), profile_(profile) {
    pref_change_registrar_.Init(profile->GetPrefs());
    // Bind device policies to corresponding instance of BulkPrintersCalculator.
    device_printers_ = BulkPrintersCalculatorFactory::Get()->GetForDevice();
    if (device_printers_ && ++(BindingsCount()[device_printers_.get()]) == 1) {
      BindSettings(kDeviceNativePrintersAccessMode,
                   &CalculatorsPoliciesBinderImpl::UpdateDeviceAccessMode);
      BindSettings(kDeviceNativePrintersBlacklist,
                   &CalculatorsPoliciesBinderImpl::UpdateDeviceBlacklist);
      BindSettings(kDeviceNativePrintersWhitelist,
                   &CalculatorsPoliciesBinderImpl::UpdateDeviceWhitelist);
    }
    // Calculate account_id_.
    const user_manager::User* user =
        ProfileHelper::Get()->GetUserByProfile(profile);
    if (user) {
      account_id_ = user->GetAccountId();
      user_printers_ =
          BulkPrintersCalculatorFactory::Get()->GetForAccountId(account_id_);
    }
    // Bind user policies to corresponding instance of BulkPrintersCalculator.
    if (user_printers_ && ++(BindingsCount()[user_printers_.get()]) == 1) {
      BindPref(prefs::kRecommendedNativePrintersAccessMode,
               &CalculatorsPoliciesBinderImpl::UpdateUserAccessMode);
      BindPref(prefs::kRecommendedNativePrintersBlacklist,
               &CalculatorsPoliciesBinderImpl::UpdateUserBlacklist);
      BindPref(prefs::kRecommendedNativePrintersWhitelist,
               &CalculatorsPoliciesBinderImpl::UpdateUserWhitelist);
    }
  }

  ~CalculatorsPoliciesBinderImpl() override {
    // We have to decrease counters in bindings_count.
    if (device_printers_ && --(BindingsCount()[device_printers_.get()]) == 0) {
      BindingsCount().erase(device_printers_.get());
    }
    if (user_printers_ && --(BindingsCount()[user_printers_.get()]) == 0) {
      BindingsCount().erase(user_printers_.get());
      BulkPrintersCalculatorFactory::Get()->RemoveForUserId(account_id_);
    }
  }

 private:
  // Methods propagating values from policies to BulkPrintersCalculator.
  void UpdateDeviceAccessMode() {
    int mode_val;
    if (!settings_->GetInteger(kDeviceNativePrintersAccessMode, &mode_val)) {
      mode_val = BulkPrintersCalculator::AccessMode::UNSET;
    }
    if (device_printers_) {
      device_printers_->SetAccessMode(ConvertToAccessMode(mode_val));
    }
  }

  void UpdateDeviceBlacklist() {
    if (device_printers_) {
      device_printers_->SetBlacklist(
          FromSettings(kDeviceNativePrintersBlacklist));
    }
  }

  void UpdateDeviceWhitelist() {
    if (device_printers_) {
      device_printers_->SetWhitelist(
          FromSettings(kDeviceNativePrintersWhitelist));
    }
  }

  void UpdateUserAccessMode() {
    if (user_printers_) {
      user_printers_->SetAccessMode(
          ConvertToAccessMode(profile_->GetPrefs()->GetInteger(
              prefs::kRecommendedNativePrintersAccessMode)));
    }
  }

  void UpdateUserBlacklist() {
    if (user_printers_) {
      user_printers_->SetBlacklist(
          FromPrefs(prefs::kRecommendedNativePrintersBlacklist));
    }
  }

  void UpdateUserWhitelist() {
    if (user_printers_) {
      user_printers_->SetWhitelist(
          FromPrefs(prefs::kRecommendedNativePrintersWhitelist));
    }
  }

  typedef void (CalculatorsPoliciesBinderImpl::*SimpleMethod)();

  // Binds given device policy to given method and calls this method once.
  void BindPref(const char* policy_name, SimpleMethod method_to_call) {
    pref_change_registrar_.Add(
        policy_name,
        base::BindRepeating(method_to_call, base::Unretained(this)));
    (this->*method_to_call)();
  }

  // Binds given user policy to given method and calls this method once.
  void BindSettings(const char* policy_name, SimpleMethod method_to_call) {
    subscriptions_.push_back(settings_->AddSettingsObserver(
        policy_name,
        base::BindRepeating(method_to_call, base::Unretained(this))));
    (this->*method_to_call)();
  }

  // Extracts the list of strings named |policy_name| from device policies.
  std::vector<std::string> FromSettings(const std::string& policy_name) {
    const base::ListValue* list;
    if (!settings_->GetList(policy_name, &list)) {
      list = nullptr;
    }
    return ConvertToVector(list);
  }

  // Extracts the list of strings named |policy_name| from user policies.
  std::vector<std::string> FromPrefs(const std::string& policy_name) {
    return ConvertToVector(profile_->GetPrefs()->GetList(policy_name));
  }

  // Device and user bulk printers calculator. Unowned. They both may be set to
  // nullptr during system shutdown. The user bulk printers calculator is also
  // set to nullptr when corresponding profile is being destroyed.
  base::WeakPtr<BulkPrintersCalculator> device_printers_;
  base::WeakPtr<BulkPrintersCalculator> user_printers_;

  // Device and profile (user) settings.
  CrosSettings* settings_;
  std::list<std::unique_ptr<CrosSettings::ObserverSubscription>> subscriptions_;
  Profile* profile_;
  AccountId account_id_;
  PrefChangeRegistrar pref_change_registrar_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(CalculatorsPoliciesBinderImpl);
};

}  // namespace

// static
void CalculatorsPoliciesBinder::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Default value for access mode is AllAccess.
  registry->RegisterIntegerPref(prefs::kRecommendedNativePrintersAccessMode,
                                BulkPrintersCalculator::ALL_ACCESS);
  registry->RegisterListPref(prefs::kRecommendedNativePrintersBlacklist);
  registry->RegisterListPref(prefs::kRecommendedNativePrintersWhitelist);
}

// static
std::unique_ptr<CalculatorsPoliciesBinder> CalculatorsPoliciesBinder::Create(
    CrosSettings* settings,
    Profile* profile) {
  return std::make_unique<CalculatorsPoliciesBinderImpl>(settings, profile);
}

}  // namespace chromeos
