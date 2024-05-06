// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/calculators_policies_binder.h"

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator.h"
#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

namespace ash {

namespace {

BulkPrintersCalculator::AccessMode ConvertToAccessMode(int mode_val) {
  if (mode_val >= BulkPrintersCalculator::BLOCKLIST_ONLY &&
      mode_val <= BulkPrintersCalculator::ALL_ACCESS) {
    return static_cast<BulkPrintersCalculator::AccessMode>(mode_val);
  }
  // Error occurred, let's return the default value.
  LOG(ERROR) << "Unrecognized access mode";
  return BulkPrintersCalculator::ALL_ACCESS;
}

std::vector<std::string> ConvertToVector(const base::Value::List& list) {
  std::vector<std::string> string_list;

  for (const base::Value& value : list) {
    if (value.is_string()) {
      string_list.push_back(value.GetString());
    }
  }
  return string_list;
}

class PrefBinder : public CalculatorsPoliciesBinder {
 public:
  PrefBinder(PrefService* pref_service,
             base::WeakPtr<BulkPrintersCalculator> calculator)
      : CalculatorsPoliciesBinder(prefs::kRecommendedPrintersAccessMode,
                                  prefs::kRecommendedPrintersBlocklist,
                                  prefs::kRecommendedPrintersAllowlist,
                                  calculator),
        prefs_(pref_service) {
    pref_change_registrar_.Init(prefs_);
  }

 protected:
  void Bind(const char* policy_name, base::RepeatingClosure closure) override {
    DVLOG(1) << "Binding " << policy_name;
    pref_change_registrar_.Add(policy_name, closure);
  }

  int GetAccessMode(const char* name) const override {
    return prefs_->GetInteger(name);
  }

  std::vector<std::string> GetStringList(const char* name) const override {
    return ConvertToVector(prefs_->GetList(name));
  }

 private:
  raw_ptr<PrefService> prefs_;
  PrefChangeRegistrar pref_change_registrar_;
};

class SettingsBinder : public CalculatorsPoliciesBinder {
 public:
  SettingsBinder(CrosSettings* settings,
                 base::WeakPtr<BulkPrintersCalculator> calculator)
      : CalculatorsPoliciesBinder(kDevicePrintersAccessMode,
                                  kDevicePrintersBlocklist,
                                  kDevicePrintersAllowlist,
                                  calculator),
        settings_(settings) {}

 protected:
  void Bind(const char* policy_name, base::RepeatingClosure closure) override {
    DVLOG(1) << "Bind device setting: " << policy_name;
    subscriptions_.push_back(
        settings_->AddSettingsObserver(policy_name, closure));
  }

  int GetAccessMode(const char* name) const override {
    int mode_val;
    if (!settings_->GetInteger(name, &mode_val)) {
      mode_val = BulkPrintersCalculator::AccessMode::UNSET;
    }
    DVLOG(1) << "Device access mode: " << mode_val;
    return mode_val;
  }

  std::vector<std::string> GetStringList(const char* name) const override {
    const base::Value* pref = settings_->GetPref(name);
    return pref && pref->is_list() ? ConvertToVector(pref->GetList())
                                   : std::vector<std::string>();
  }

 private:
  raw_ptr<CrosSettings> settings_;
  std::list<base::CallbackListSubscription> subscriptions_;
};

}  // namespace

// static
void CalculatorsPoliciesBinder::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  // Default value for access mode is AllAccess.
  registry->RegisterIntegerPref(prefs::kRecommendedPrintersAccessMode,
                                BulkPrintersCalculator::ALL_ACCESS);
  registry->RegisterListPref(prefs::kRecommendedPrintersBlocklist);
  registry->RegisterListPref(prefs::kRecommendedPrintersAllowlist);
}

// static
std::unique_ptr<CalculatorsPoliciesBinder>
CalculatorsPoliciesBinder::DeviceBinder(
    CrosSettings* settings,
    base::WeakPtr<BulkPrintersCalculator> calculator) {
  auto binder = std::make_unique<SettingsBinder>(settings, calculator);
  binder->Init();
  return binder;
}

// static
std::unique_ptr<CalculatorsPoliciesBinder>
CalculatorsPoliciesBinder::UserBinder(
    PrefService* prefs,
    base::WeakPtr<BulkPrintersCalculator> calculator) {
  auto binder = std::make_unique<PrefBinder>(prefs, calculator);
  binder->Init();
  return binder;
}

CalculatorsPoliciesBinder::CalculatorsPoliciesBinder(
    const char* access_mode_name,
    const char* blocklist_name,
    const char* allowlist_name,
    base::WeakPtr<BulkPrintersCalculator> calculator)
    : access_mode_name_(access_mode_name),
      blocklist_name_(blocklist_name),
      allowlist_name_(allowlist_name),
      calculator_(calculator) {
  DCHECK(access_mode_name);
  DCHECK(blocklist_name);
  DCHECK(allowlist_name);
  DCHECK(calculator);
}

CalculatorsPoliciesBinder::~CalculatorsPoliciesBinder() = default;

base::WeakPtr<CalculatorsPoliciesBinder>
CalculatorsPoliciesBinder::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void CalculatorsPoliciesBinder::Init() {
  // Register for future updates.
  Bind(access_mode_name_,
       base::BindRepeating(&CalculatorsPoliciesBinder::UpdateAccessMode,
                           GetWeakPtr()));
  Bind(blocklist_name_,
       base::BindRepeating(&CalculatorsPoliciesBinder::UpdateBlocklist,
                           GetWeakPtr()));
  Bind(allowlist_name_,
       base::BindRepeating(&CalculatorsPoliciesBinder::UpdateAllowlist,
                           GetWeakPtr()));

  // Retrieve initial values for all policy fields.
  UpdateAccessMode();
  UpdateBlocklist();
  UpdateAllowlist();
}

void CalculatorsPoliciesBinder::UpdateAccessMode() {
  DVLOG(1) << "Update access mode";
  if (calculator_) {
    calculator_->SetAccessMode(
        ConvertToAccessMode(GetAccessMode(access_mode_name_)));
  }
}

void CalculatorsPoliciesBinder::UpdateAllowlist() {
  if (calculator_) {
    calculator_->SetAllowlist(GetStringList(allowlist_name_));
  }
}

void CalculatorsPoliciesBinder::UpdateBlocklist() {
  if (calculator_) {
    calculator_->SetBlocklist(GetStringList(blocklist_name_));
  }
}

}  // namespace ash
