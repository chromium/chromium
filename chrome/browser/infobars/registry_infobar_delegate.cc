// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/registry_infobar_delegate.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/infobars/confirm_infobar_creation_allowlist.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/infobars/infobar_outcome_tracker.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace infobars {

RegistryInfoBarDelegate::RegistryInfoBarDelegate(
    scoped_refptr<const SharedInfoBarSpec> spec,
    content::WebContents* contents,
    InfoBarShowParams params,
    scoped_refptr<InfoBarOutcomeTracker> outcome,
    base::RepeatingClosure on_global_close)
    : content::WebContentsObserver(contents),
      spec_(std::move(spec)),
      params_(std::move(params)),
      outcome_(std::move(outcome)),
      on_global_close_(std::move(on_global_close)) {
  CHECK(spec_);
  CHECK(outcome_);
  if (params_.substitutions.has_value()) {
    substitutions_ = std::move(*params_.substitutions);
  }
}

RegistryInfoBarDelegate::~RegistryInfoBarDelegate() {
  // Tab-scoped delegates own the single logical show, so report any still-
  // pending fallback outcome directly with the current WebContents, which may
  // be null.
  // Global delegates defer fallback reporting to `BrowserInfoBarManager`.
  if (!on_global_close_) {
    outcome_->ReportPending(web_contents());
  }
}

InfoBarDelegate::InfoBarIdentifier RegistryInfoBarDelegate::GetIdentifier()
    const {
  return spec().identifier();
}

bool RegistryInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  if (spec().allow_duplicates()) {
    return false;
  }
  return ConfirmInfoBarDelegate::EqualsDelegate(delegate);
}

std::u16string RegistryInfoBarDelegate::GetMessageText() const {
  if (params_.message_text.has_value()) {
    return *params_.message_text;
  }
  if (!spec().message_text_template().empty()) {
    std::vector<std::u16string> substitution_texts;
    for (const MessageSubstitution& substitution : GetMessageSubstitutions()) {
      substitution_texts.push_back(substitution.text);
    }
    return base::ReplaceStringPlaceholders(spec().message_text_template(),
                                           substitution_texts,
                                           /*offsets=*/nullptr);
  }
  return spec().message_text();
}

std::u16string RegistryInfoBarDelegate::GetMessageTextTemplate() const {
  if (params_.message_text.has_value()) {
    return std::u16string();
  }
  return spec().message_text_template();
}

base::span<const MessageSubstitution>
RegistryInfoBarDelegate::GetMessageSubstitutions() const {
  if (!substitutions_.has_value()) {
    substitutions_ = spec().substitutions_callback()
                         ? spec().substitutions_callback().Run(web_contents())
                         : std::vector<MessageSubstitution>();
  }
  return *substitutions_;
}

std::u16string RegistryInfoBarDelegate::GetLinkText() const {
  return params_.link_text.has_value() ? *params_.link_text
                                       : spec().link_text();
}

std::optional<std::u16string> RegistryInfoBarDelegate::GetLinkAccessibleText()
    const {
  return spec().link_accessible_text();
}

GURL RegistryInfoBarDelegate::GetLinkURL() const {
  return spec().link_navigation_url();
}

bool RegistryInfoBarDelegate::ShouldShowLinkBeforeButton() const {
  return spec().should_show_link_before_button();
}

int RegistryInfoBarDelegate::GetLinkSpacingWhenPositionedBeforeButton() const {
  return spec().link_spacing_when_positioned_before_button();
}

int RegistryInfoBarDelegate::GetIconId() const {
  return spec().icon_id();
}

const gfx::VectorIcon& RegistryInfoBarDelegate::GetVectorIcon() const {
  if (params_.icon) {
    return *params_.icon;
  }
  if (dark_mode() && spec().dark_mode_icon()) {
    return *spec().dark_mode_icon();
  }
  return spec().icon() ? *spec().icon()
                       : ConfirmInfoBarDelegate::GetVectorIcon();
}

int RegistryInfoBarDelegate::GetButtons() const {
  int buttons = BUTTON_NONE;
  const std::u16string& ok_label = params_.ok_button.label.has_value()
                                       ? *params_.ok_button.label
                                       : spec().ok_button_label();
  if (!ok_label.empty() || spec().ok_button_callback() ||
      params_.ok_button_callback) {
    buttons |= BUTTON_OK;
  }
  const std::u16string& cancel_label = params_.cancel_button.label.has_value()
                                           ? *params_.cancel_button.label
                                           : spec().cancel_button_label();
  if (!cancel_label.empty() || spec().cancel_button_callback() ||
      params_.cancel_button_callback) {
    buttons |= BUTTON_CANCEL;
  }
  const std::u16string& extra_label = params_.extra_button.label.has_value()
                                          ? *params_.extra_button.label
                                          : spec().extra_button_label();
  if (!extra_label.empty() || spec().extra_button_callback() ||
      params_.extra_button_callback) {
    buttons |= BUTTON_EXTRA;
  }
  return buttons;
}

std::u16string RegistryInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    const std::u16string& label = params_.ok_button.label.has_value()
                                      ? *params_.ok_button.label
                                      : spec().ok_button_label();
    if (!label.empty()) {
      return label;
    }
  }
  if (button == BUTTON_CANCEL) {
    const std::u16string& label = params_.cancel_button.label.has_value()
                                      ? *params_.cancel_button.label
                                      : spec().cancel_button_label();
    if (!label.empty()) {
      return label;
    }
  }
  if (button == BUTTON_EXTRA) {
    const std::u16string& label = params_.extra_button.label.has_value()
                                      ? *params_.extra_button.label
                                      : spec().extra_button_label();
    if (!label.empty()) {
      return label;
    }
  }
  return ConfirmInfoBarDelegate::GetButtonLabel(button);
}

ui::ImageModel RegistryInfoBarDelegate::GetButtonImage(
    InfoBarButton button) const {
  if (const auto* p = GetButtonParams(button); p && p->image.has_value()) {
    return *p->image;
  }
  return ConfirmInfoBarDelegate::GetButtonImage(button);
}

bool RegistryInfoBarDelegate::GetButtonEnabled(InfoBarButton button) const {
  if (const auto* p = GetButtonParams(button); p && p->enabled.has_value()) {
    return *p->enabled;
  }
  return ConfirmInfoBarDelegate::GetButtonEnabled(button);
}

std::u16string RegistryInfoBarDelegate::GetButtonTooltip(
    InfoBarButton button) const {
  if (const auto* p = GetButtonParams(button); p && p->tooltip.has_value()) {
    return *p->tooltip;
  }
  return ConfirmInfoBarDelegate::GetButtonTooltip(button);
}

std::optional<ui::ButtonStyle> RegistryInfoBarDelegate::GetButtonStyle(
    InfoBarButton button) const {
  if (const auto* p = GetButtonParams(button); p && p->style.has_value()) {
    return p->style;
  }
  return ConfirmInfoBarDelegate::GetButtonStyle(button);
}

bool RegistryInfoBarDelegate::ShouldUseTextColorForButtonIcon(
    InfoBarButton button) const {
  if (const auto* p = GetButtonParams(button);
      p && p->use_text_color_for_icon.has_value()) {
    return *p->use_text_color_for_icon;
  }
  return ConfirmInfoBarDelegate::ShouldUseTextColorForButtonIcon(button);
}

bool RegistryInfoBarDelegate::Accept() {
  base::UmaHistogramSparse("InfoBar.Centralized.Accept", GetIdentifier());
  return RunAction(params_.ok_button_callback ? params_.ok_button_callback
                                              : spec().ok_button_callback(),
                   InfoBarResult::kAccepted, spec().close_on_accept());
}

bool RegistryInfoBarDelegate::Cancel() {
  base::UmaHistogramSparse("InfoBar.Centralized.Cancel", GetIdentifier());
  return RunAction(params_.cancel_button_callback
                       ? params_.cancel_button_callback
                       : spec().cancel_button_callback(),
                   InfoBarResult::kCancelled, spec().close_on_cancel());
}

bool RegistryInfoBarDelegate::ExtraButtonPressed() {
  base::UmaHistogramSparse("InfoBar.Centralized.Extra", GetIdentifier());
  return RunAction(
      params_.extra_button_callback ? params_.extra_button_callback
                                    : spec().extra_button_callback(),
      InfoBarResult::kExtraButtonPressed, spec().close_on_extra_button());
}

void RegistryInfoBarDelegate::InfoBarDismissed() {
  base::UmaHistogramSparse("InfoBar.Centralized.Dismiss", GetIdentifier());
  RunAction(spec().dismiss_callback(), InfoBarResult::kDismissed);
}

bool RegistryInfoBarDelegate::LinkClicked(
    ui::mojom::WindowOpenDisposition disposition) {
  in_interaction_ = true;
  outcome_->MarkLinkClicked();
  base::UmaHistogramSparse("InfoBar.Centralized.LinkClicked", GetIdentifier());
  auto weak_this = weak_factory_.GetWeakPtr();
  const bool should_close = ConfirmInfoBarDelegate::LinkClicked(disposition);
  if (!weak_this) {
    return false;
  }
  in_interaction_ = false;
  return should_close || close_after_interaction_;
}

bool RegistryInfoBarDelegate::InlineSubstitutionLinkClicked(
    size_t index,
    ui::mojom::WindowOpenDisposition disposition) {
  in_interaction_ = true;
  outcome_->MarkLinkClicked();
  base::UmaHistogramSparse("InfoBar.Centralized.LinkClicked", GetIdentifier());

  const InfoBarSpec::InlineLinkCallback callback =
      params_.inline_link_callback ? params_.inline_link_callback
                                   : spec().inline_link_callback();
  scoped_refptr<InfoBarOutcomeTracker> outcome = outcome_;
  base::RepeatingClosure on_global_close = on_global_close_;
  auto weak_this = weak_factory_.GetWeakPtr();
  content::WebContents* contents = web_contents();
  base::WeakPtr<content::WebContents> weak_contents =
      contents ? contents->GetWeakPtr() : nullptr;

  const bool should_close =
      callback && callback.Run(contents, index, disposition);
  if (should_close) {
    InfoBarOutcomeTracker::ClaimedCallback report =
        outcome->Claim(InfoBarResult::kLinkClicked);
    if (on_global_close) {
      on_global_close.Run();
    }
    if (report) {
      std::move(report).Run(weak_contents.get());
    }
  }
  if (!weak_this) {
    return false;
  }
  in_interaction_ = false;
  return should_close || close_after_interaction_;
}

bool RegistryInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return spec().expire_on_navigation() &&
         ConfirmInfoBarDelegate::ShouldExpire(details);
}

bool RegistryInfoBarDelegate::ShouldHideInFullscreen() const {
  return spec().should_hide_in_fullscreen();
}

bool RegistryInfoBarDelegate::ShouldAnimate() const {
  return spec().should_animate();
}

bool RegistryInfoBarDelegate::IsCloseable() const {
  return spec().is_closeable();
}

InfoBarDelegate::InfobarPriority RegistryInfoBarDelegate::GetPriority() const {
  return spec().priority();
}

const InfoBarButtonParams* RegistryInfoBarDelegate::GetButtonParams(
    InfoBarButton button) const {
  switch (button) {
    case BUTTON_OK:
      return &params_.ok_button;
    case BUTTON_CANCEL:
      return &params_.cancel_button;
    case BUTTON_EXTRA:
      return &params_.extra_button;
    case BUTTON_NONE:
      return nullptr;
  }
  return nullptr;
}

bool RegistryInfoBarDelegate::RunAction(InfoBarSpec::ActionCallback callback,
                                        InfoBarResult result,
                                        bool should_close) {
  in_interaction_ = true;
  auto weak_this = weak_factory_.GetWeakPtr();
  content::WebContents* contents = web_contents();
  base::WeakPtr<content::WebContents> weak_contents =
      contents ? contents->GetWeakPtr() : nullptr;
  scoped_refptr<InfoBarOutcomeTracker> outcome = outcome_;
  base::RepeatingClosure on_global_close = on_global_close_;

  InfoBarOutcomeTracker::ClaimedCallback report = outcome->Claim(result);
  if (should_close && on_global_close) {
    on_global_close.Run();
  }
  if (weak_contents && callback) {
    callback.Run(weak_contents.get());
  }
  if (report) {
    std::move(report).Run(weak_contents.get());
  }
  if (!weak_this) {
    return false;
  }
  in_interaction_ = false;
  return should_close || close_after_interaction_;
}

std::unique_ptr<InfoBar> CreateRegistryInfoBar(
    ConfirmInfoBarCreationAllowlist::PassKey pass_key,
    scoped_refptr<const SharedInfoBarSpec> spec,
    content::WebContents* contents,
    InfoBarShowParams params,
    scoped_refptr<InfoBarOutcomeTracker> outcome,
    base::RepeatingClosure on_global_close) {
  CHECK(spec);
  const InfoBarSpec::CustomViewCallback& custom_view_callback =
      params.custom_view_callback ? params.custom_view_callback
                                  : spec->data.custom_view_callback();
  std::unique_ptr<views::View> custom_view =
      custom_view_callback ? custom_view_callback.Run(contents) : nullptr;
  return CreateConfirmInfoBar(
      pass_key,
      std::make_unique<RegistryInfoBarDelegate>(
          std::move(spec), contents, std::move(params), std::move(outcome),
          std::move(on_global_close)),
      std::move(custom_view));
}

void RemoveRegistryInfoBar(InfoBarManager* manager,
                           InfoBar* infobar,
                           bool suppress_outcome) {
  auto* delegate = static_cast<RegistryInfoBarDelegate*>(infobar->delegate());
  if (suppress_outcome) {
    delegate->outcome()->Suppress();
  }
  if (delegate->in_interaction()) {
    delegate->set_close_after_interaction();
    return;
  }
  manager->RemoveInfoBar(infobar);
}

}  // namespace infobars
