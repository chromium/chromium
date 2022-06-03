// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/font_pref_change_notifier.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "chrome/common/pref_names_util.h"
#include "components/prefs/pref_service.h"

FontPrefChangeNotifier::Registrar::Registrar() {}
FontPrefChangeNotifier::Registrar::~Registrar() {
  if (is_registered())
    Unregister();
}

void FontPrefChangeNotifier::Registrar::Register(
    FontPrefChangeNotifier* notifier,
    FontPrefChangeNotifier::Callback cb) {
  DCHECK(!is_registered());
  notifier_ = notifier;
  callback_ = std::move(cb);

  notifier_->AddRegistrar(this);
}

void FontPrefChangeNotifier::Registrar::Unregister() {
  DCHECK(is_registered());
  notifier_->RemoveRegistrar(this);

  notifier_ = nullptr;
  callback_ = FontPrefChangeNotifier::Callback();
}

FontPrefChangeNotifier::FontPrefChangeNotifier(PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_service_->AddPrefObserverAllPrefs(this);
}

FontPrefChangeNotifier::~FontPrefChangeNotifier() {
  pref_service_->RemovePrefObserverAllPrefs(this);

  // There could be a shutdown race between this class and the objects
  // registered with it. We don't want the registrars to call back into us
  // when we're deleted, so tell them to unregister now.
  for (auto& reg : registrars_)
    reg.Unregister();
}

void FontPrefChangeNotifier::AddRegistrar(Registrar* registrar) {
  registrars_.AddObserver(registrar);
}

void FontPrefChangeNotifier::RemoveRegistrar(Registrar* registrar) {
  registrars_.RemoveObserver(registrar);
}

void FontPrefChangeNotifier::OnPreferenceChanged(PrefService* pref_service,
                                                 const std::string& pref_name) {
  if (base::StartsWith(pref_name, pref_names_util::kWebKitFontPrefPrefix,
                       base::CompareCase::SENSITIVE)) {
    for (auto& reg : registrars_)
      reg.callback_.Run(pref_name);
  }
}
