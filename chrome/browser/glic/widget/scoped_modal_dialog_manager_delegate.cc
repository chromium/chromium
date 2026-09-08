// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/scoped_modal_dialog_manager_delegate.h"

#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"

namespace glic {

ScopedModalDialogManagerDelegate::ScopedModalDialogManagerDelegate(
    web_modal::WebContentsModalDialogManagerDelegate* delegate)
    : delegate_(delegate) {
  CHECK(delegate_);
}

ScopedModalDialogManagerDelegate::~ScopedModalDialogManagerDelegate() {
  Reset();
}

void ScopedModalDialogManagerDelegate::SetWebContents(
    content::WebContents* new_web_contents) {
  if (web_contents() == new_web_contents) {
    return;
  }

  Reset();

  if (new_web_contents) {
    Observe(new_web_contents);
    web_modal::WebContentsModalDialogManager::CreateForWebContents(
        new_web_contents);
    if (auto* dialog_manager =
            web_modal::WebContentsModalDialogManager::FromWebContents(
                new_web_contents)) {
      dialog_manager->SetDelegate(delegate_);
    }
  }
}

void ScopedModalDialogManagerDelegate::Reset() {
  if (content::WebContents* current = web_contents()) {
    if (auto* dialog_manager =
            web_modal::WebContentsModalDialogManager::FromWebContents(
                current)) {
      if (dialog_manager->delegate() == delegate_) {
        dialog_manager->SetDelegate(nullptr);
      }
    }
    Observe(nullptr);
  }
}

}  // namespace glic
