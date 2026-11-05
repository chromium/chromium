// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/browser_ui/glic_toast.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {

namespace {

class GlicToastImpl : public GlicToast {
 public:
  GlicToastImpl(const tabs::TabInterface& tab,
                int title_res_id,
                int description_res_id) {
    const std::u16string body_text =
        (description_res_id != 0)
            ? l10n_util::GetStringFUTF16(
                  IDS_GLIC_TOAST_TITLE_AND_DESCRIPTION,
                  l10n_util::GetStringUTF16(title_res_id),
                  l10n_util::GetStringUTF16(description_res_id))
            : l10n_util::GetStringUTF16(title_res_id);

    if (auto* toast_controller =
            ToastController::MaybeGetForTabInterface(tab.GetHandle().Get())) {
      ToastParams params(ToastId::kGlic);
      params.body_string_override = body_text;
      params.toast_close_callback = base::ScopedClosureRunner(base::BindOnce(
          &GlicToastImpl::HandleToastClosed, weak_ptr_factory_.GetWeakPtr()));
      is_showing_ = true;
      if (!toast_controller->MaybeShowToast(std::move(params))) {
        is_showing_ = false;
      }
    }
  }

  ~GlicToastImpl() override = default;

  bool IsShowing() const override { return is_showing_; }

 private:
  void HandleToastClosed() { is_showing_ = false; }

  bool is_showing_ = false;
  base::WeakPtrFactory<GlicToastImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<GlicToast> GlicToast::Show(const tabs::TabInterface& tab,
                                           int title_res_id,
                                           int description_res_id) {
  if (!tab.IsActivated()) {
    return nullptr;
  }
  auto toast =
      std::make_unique<GlicToastImpl>(tab, title_res_id, description_res_id);
  if (!toast->IsShowing()) {
    return nullptr;
  }
  return toast;
}

}  // namespace glic
