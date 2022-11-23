// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/prefs_ash_observer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"

PrefsAshObserver::PrefsAshObserver(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

PrefsAshObserver::~PrefsAshObserver() = default;

void PrefsAshObserver::Init() {
  // Initial values are obtained when the observers are created, there is no
  // need to do so explcitly.
  doh_mode_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kDnsOverHttpsMode,
      base::BindRepeating(&PrefsAshObserver::OnDnsOverHttpsModeChanged,
                          base::Unretained(this)));
  doh_templates_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kDnsOverHttpsTemplates,
      base::BindRepeating(&PrefsAshObserver::OnDnsOverHttpsTemplatesChanged,
                          base::Unretained(this)));
  doh_templates_with_identifiers_observer_ =
      std::make_unique<CrosapiPrefObserver>(
          crosapi::mojom::PrefPath::kDnsOverHttpsTemplatesWithIdentifiers,
          base::BindRepeating(
              &PrefsAshObserver::OnDnsOverHttpsTemplatesWithIdentifiersChanged,
              base::Unretained(this)));
  doh_salt_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kDnsOverHttpsSalt,
      base::BindRepeating(&PrefsAshObserver::OnDnsOverHttpsSaltChanged,
                          base::Unretained(this)));
}

void PrefsAshObserver::OnDnsOverHttpsModeChanged(base::Value value) {
  if (!value.is_string()) {
    LOG(WARNING) << "Unexpected value type: "
                 << base::Value::GetTypeName(value.type());
    return;
  }
  local_state_->SetString(prefs::kDnsOverHttpsMode, value.GetString());
}

void PrefsAshObserver::OnDnsOverHttpsTemplatesChanged(base::Value value) {
  if (!value.is_string()) {
    LOG(WARNING) << "Unexpected value type: "
                 << base::Value::GetTypeName(value.type());
    return;
  }
  local_state_->SetString(prefs::kDnsOverHttpsTemplates, value.GetString());
}

void PrefsAshObserver::OnDnsOverHttpsTemplatesWithIdentifiersChanged(
    base::Value value) {
  if (!value.is_string()) {
    LOG(WARNING) << "Unexpected value type: "
                 << base::Value::GetTypeName(value.type());
    return;
  }

  local_state_->SetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                          value.GetString());
}

void PrefsAshObserver::OnDnsOverHttpsSaltChanged(base::Value value) {
  if (!value.is_string()) {
    LOG(WARNING) << "Unexpected value type: "
                 << base::Value::GetTypeName(value.type());
    return;
  }

  local_state_->SetString(prefs::kDnsOverHttpsSalt, value.GetString());
}
