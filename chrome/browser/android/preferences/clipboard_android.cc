// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/clipboard_android.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_android.h"

namespace {

void HandleClipboardModified(PrefService* local_state, base::Time time) {
  local_state->SetInt64(prefs::kClipboardLastModifiedTime,
                        time.ToInternalValue());
}

}  // namespace

namespace android {

void RegisterClipboardAndroidPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kClipboardLastModifiedTime, 0u);
}

void InitClipboardAndroidFromLocalState(PrefService* local_state) {
  DCHECK(local_state);
  // Given the context, the cast is guaranteed to succeed.
  ui::ClipboardAndroid* clipboard =
      static_cast<ui::ClipboardAndroid*>(ui::Clipboard::GetForCurrentThread());
  clipboard->SetLastModifiedTimeWithoutRunningCallback(
      base::Time::FromInternalValue(
          local_state->GetInt64(prefs::kClipboardLastModifiedTime)));
  clipboard->SetModifiedCallback(
      base::BindRepeating(&HandleClipboardModified, local_state));
}

}  // namespace android
