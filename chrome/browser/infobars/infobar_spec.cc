// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_spec.h"

#include <string>
#include <utility>

#include "components/infobars/core/infobar_delegate.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace infobars {

InfoBarSpec::Builder::Builder(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  spec_.identifier_ = identifier;
}

InfoBarSpec::Builder::~Builder() = default;

InfoBarSpec::Builder& InfoBarSpec::Builder::SetMessageText(
    std::u16string message_text) {
  spec_.message_text_ = std::move(message_text);
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetMessageTextTemplate(
    std::u16string message_text_template) {
  spec_.message_text_template_ = std::move(message_text_template);
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetSubstitutionsCallback(
    SubstitutionsCallback callback) {
  spec_.substitutions_callback_ = std::move(callback);
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetInlineLinkCallback(
    InlineLinkCallback callback) {
  spec_.inline_link_callback_ = std::move(callback);
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetLinkText(
    std::u16string link_text) {
  spec_.link_text_ = std::move(link_text);
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetLinkNavigationUrl(GURL gurl) {
  spec_.link_navigation_url_ = std::move(gurl);
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetIcon(
    const gfx::VectorIcon& icon) {
  spec_.icon_ = &icon;
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetIconId(int icon_id) {
  spec_.icon_id_ = icon_id;
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetScope(InfoBarScope scope) {
  spec_.scope_ = scope;
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetPriority(
    InfoBarDelegate::InfobarPriority priority) {
  spec_.priority_ = priority;
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetExpireOnNavigation(
    bool expire_on_navigation) {
  spec_.expire_on_navigation_ = expire_on_navigation;
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetShouldHideInFullscreen(
    bool should_hide_in_fullscreen) {
  spec_.should_hide_in_fullscreen_ = should_hide_in_fullscreen;
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetShouldAnimate(
    bool should_animate) {
  spec_.should_animate_ = should_animate;
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetIsCloseable(bool is_closeable) {
  spec_.is_closeable_ = is_closeable;
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::AddOkButton(
    const std::u16string& label,
    ActionCallback callback) {
  spec_.ok_button_label_ = label;
  spec_.ok_button_callback_ = std::move(callback);
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::AddCancelButton(
    const std::u16string& label,
    ActionCallback callback) {
  spec_.cancel_button_label_ = label;
  spec_.cancel_button_callback_ = std::move(callback);
  return *this;
}

InfoBarSpec::Builder& InfoBarSpec::Builder::SetDismissAction(
    ActionCallback callback) {
  spec_.dismiss_callback_ = std::move(callback);
  return *this;
}

InfoBarSpec InfoBarSpec::Builder::Build() {
  return std::move(spec_);
}

InfoBarSpec::InfoBarSpec() = default;
InfoBarSpec::InfoBarSpec(const InfoBarSpec&) = default;
InfoBarSpec::InfoBarSpec(InfoBarSpec&&) = default;
InfoBarSpec::~InfoBarSpec() = default;
InfoBarSpec& InfoBarSpec::operator=(const InfoBarSpec&) = default;

}  // namespace infobars
