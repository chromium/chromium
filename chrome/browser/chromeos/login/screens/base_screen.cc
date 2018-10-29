// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/base_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/model_view_channel.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

BaseScreen::ContextEditor::ContextEditor(BaseScreen& screen)
    : screen_(screen), context_(screen.context_) {}

BaseScreen::ContextEditor::~ContextEditor() {
  screen_.CommitContextChanges();
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetBoolean(
    const KeyType& key,
    bool value) const {
  context_.SetBoolean(key, value);
  return *this;
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetInteger(
    const KeyType& key,
    int value) const {
  context_.SetInteger(key, value);
  return *this;
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetDouble(
    const KeyType& key,
    double value) const {
  context_.SetDouble(key, value);
  return *this;
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetString(
    const KeyType& key,
    const std::string& value) const {
  context_.SetString(key, value);
  return *this;
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetString(
    const KeyType& key,
    const base::string16& value) const {
  context_.SetString(key, value);
  return *this;
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetStringList(
    const KeyType& key,
    const StringList& value) const {
  context_.SetStringList(key, value);
  return *this;
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetString16List(
    const KeyType& key,
    const String16List& value) const {
  context_.SetString16List(key, value);
  return *this;
}

BaseScreen::BaseScreen(BaseScreenDelegate* base_screen_delegate,
                       OobeScreen screen_id)
    : base_screen_delegate_(base_screen_delegate), screen_id_(screen_id) {}

BaseScreen::~BaseScreen() {}

void BaseScreen::OnShow() {}

void BaseScreen::OnHide() {}

void BaseScreen::OnClose() {}

void BaseScreen::OnConfigurationChanged() {}

bool BaseScreen::IsStatusAreaDisplayed() {
  return true;
}

void BaseScreen::CommitContextChanges() {
  if (!context_.HasChanges())
    return;
  if (!channel_) {
    LOG(ERROR) << "Model-view channel for " << GetOobeScreenName(screen_id())
               << " is not ready, context changes are not sent to the view.";
    return;
  }
  base::DictionaryValue diff;
  context_.GetChangesAndReset(&diff);
  channel_->CommitContextChanges(diff);
}

void BaseScreen::Finish(ScreenExitCode exit_code) {
  base_screen_delegate_->OnExit(exit_code);
}

bool BaseScreen::IsPublicSessionOrEphemeralLogin() {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  return user_manager->IsLoggedInAsPublicAccount() ||
         (user_manager->IsCurrentUserNonCryptohomeDataEphemeral() &&
          user_manager->GetActiveUser()->GetType() !=
              user_manager::USER_TYPE_REGULAR);
}

void BaseScreen::OnUserAction(const std::string& action_id) {
  LOG(WARNING) << "Unhandled user action: action_id=" << action_id;
}

void BaseScreen::OnContextKeyUpdated(
    const ::login::ScreenContext::KeyType& key) {
  LOG(WARNING) << "Unhandled context change: key=" << key;
}

BaseScreen::ContextEditor BaseScreen::GetContextEditor() {
  return ContextEditor(*this);
}

void BaseScreen::OnContextChanged(const base::DictionaryValue& diff) {
  std::vector<::login::ScreenContext::KeyType> keys;
  context_.ApplyChanges(diff, &keys);
  for (const auto& key : keys)
    OnContextKeyUpdated(key);
}

void BaseScreen::SetConfiguration(base::Value* configuration, bool notify) {
  configuration_ = configuration;
  if (notify)
    OnConfigurationChanged();
}

}  // namespace chromeos
