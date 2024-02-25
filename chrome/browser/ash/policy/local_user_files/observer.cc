// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/local_user_files/observer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"

namespace policy::local_user_files {

Observer::Observer()
    : pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  pref_change_registrar_->Init(g_browser_process->local_state());
  pref_change_registrar_->Add(
      prefs::kLocalUserFilesAllowed,
      base::BindRepeating(&Observer::OnLocalUserFilesPolicyChanged,
                          base::Unretained(this)));
}

Observer::~Observer() {
  pref_change_registrar_->RemoveAll();
}

}  // namespace policy::local_user_files
