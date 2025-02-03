// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/android_data_controls_dialog_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/enterprise/data_controls/android_data_controls_dialog.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

namespace data_controls {

// static
AndroidDataControlsDialogFactory*
AndroidDataControlsDialogFactory::GetInstance() {
  return base::Singleton<AndroidDataControlsDialogFactory>::get();
}

void AndroidDataControlsDialogFactory::ShowDialogIfNeeded(
    content::WebContents* web_contents,
    DataControlsDialog::Type type) {
  DataControlsDialogFactory::ShowDialogIfNeeded(web_contents, type);
}

void AndroidDataControlsDialogFactory::ShowDialogIfNeeded(
    content::WebContents* web_contents,
    DataControlsDialog::Type type,
    base::OnceCallback<void(bool bypassed)> callback) {
  if (type == DataControlsDialog::Type::kClipboardPasteBlock ||
      type == DataControlsDialog::Type::kClipboardCopyBlock) {
    // Show a toast on Clank for blocked actions instead of a dialog to be less
    // disruptive.
    web_contents->GetTopLevelNativeWindow()->ShowToast(
        l10n_util::GetStringUTF8(IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION));
    return;
  }
  DataControlsDialogFactory::ShowDialogIfNeeded(web_contents, type,
                                                std::move(callback));
}

DataControlsDialog* AndroidDataControlsDialogFactory::CreateDialog(
    DataControlsDialog::Type type,
    content::WebContents* web_contents,
    base::OnceCallback<void(bool bypassed)> callback) {
  return new AndroidDataControlsDialog(type, web_contents, std::move(callback));
}

}  // namespace data_controls
