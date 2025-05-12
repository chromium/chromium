// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_interface_android.h"

#include "chrome/browser/android/tab_android.h"
#include "components/tabs/public/tab_interface.h"

TabInterfaceAndroid::TabInterfaceAndroid(TabAndroid* tab_android)
    : weak_tab_android_(tab_android->GetTabAndroidWeakPtr()) {}

TabInterfaceAndroid::~TabInterfaceAndroid() = default;

base::WeakPtr<tabs::TabInterface> TabInterfaceAndroid::GetWeakPtr() {
  if (!weak_tab_android_) {
    return nullptr;
  }
  return weak_tab_android_->GetWeakPtr();
}

content::WebContents* TabInterfaceAndroid::GetContents() const {
  if (!weak_tab_android_) {
    return nullptr;
  }
  return weak_tab_android_->GetContents();
}

void TabInterfaceAndroid::Close() {
  if (!weak_tab_android_) {
    return;
  }
  return weak_tab_android_->Close();
}

base::CallbackListSubscription TabInterfaceAndroid::RegisterWillDiscardContents(
    WillDiscardContentsCallback callback) {
  if (!weak_tab_android_) {
    return base::CallbackListSubscription();
  }
  return weak_tab_android_->RegisterWillDiscardContents(std::move(callback));
}

bool TabInterfaceAndroid::IsActivated() const {
  if (!weak_tab_android_) {
    return false;
  }
  return weak_tab_android_->IsActivated();
}

base::CallbackListSubscription TabInterfaceAndroid::RegisterDidActivate(
    DidActivateCallback callback) {
  if (!weak_tab_android_) {
    return base::CallbackListSubscription();
  }
  return weak_tab_android_->RegisterDidActivate(std::move(callback));
}

base::CallbackListSubscription TabInterfaceAndroid::RegisterWillDeactivate(
    WillDeactivateCallback callback) {
  if (!weak_tab_android_) {
    return base::CallbackListSubscription();
  }
  return weak_tab_android_->RegisterWillDeactivate(std::move(callback));
}

bool TabInterfaceAndroid::IsVisible() const {
  if (!weak_tab_android_) {
    return false;
  }
  return weak_tab_android_->IsVisible();
}

base::CallbackListSubscription TabInterfaceAndroid::RegisterDidBecomeVisible(
    DidBecomeVisibleCallback callback) {
  if (!weak_tab_android_) {
    return base::CallbackListSubscription();
  }
  return weak_tab_android_->RegisterDidBecomeVisible(std::move(callback));
}

base::CallbackListSubscription TabInterfaceAndroid::RegisterWillBecomeHidden(
    WillBecomeHiddenCallback callback) {
  if (!weak_tab_android_) {
    return base::CallbackListSubscription();
  }
  return weak_tab_android_->RegisterWillBecomeHidden(std::move(callback));
}

base::CallbackListSubscription TabInterfaceAndroid::RegisterWillDetach(
    WillDetach callback) {
  if (!weak_tab_android_) {
    return base::CallbackListSubscription();
  }
  return weak_tab_android_->RegisterWillDetach(std::move(callback));
}

base::CallbackListSubscription TabInterfaceAndroid::RegisterDidInsert(
    DidInsertCallback callback) {
  if (!weak_tab_android_) {
    return base::CallbackListSubscription();
  }
  return weak_tab_android_->RegisterDidInsert(std::move(callback));
}

base::CallbackListSubscription TabInterfaceAndroid::RegisterPinnedStateChanged(
    PinnedStateChangedCallback callback) {
  if (!weak_tab_android_) {
    return base::CallbackListSubscription();
  }
  return weak_tab_android_->RegisterPinnedStateChanged(std::move(callback));
}

base::CallbackListSubscription TabInterfaceAndroid::RegisterGroupChanged(
    GroupChangedCallback callback) {
  if (!weak_tab_android_) {
    return base::CallbackListSubscription();
  }
  return weak_tab_android_->RegisterGroupChanged(std::move(callback));
}

bool TabInterfaceAndroid::CanShowModalUI() const {
  if (!weak_tab_android_) {
    return false;
  }
  return weak_tab_android_->CanShowModalUI();
}

std::unique_ptr<tabs::ScopedTabModalUI> TabInterfaceAndroid::ShowModalUI() {
  if (!weak_tab_android_) {
    return nullptr;
  }
  return weak_tab_android_->ShowModalUI();
}

base::CallbackListSubscription TabInterfaceAndroid::RegisterModalUIChanged(
    TabInterfaceCallback callback) {
  if (!weak_tab_android_) {
    return base::CallbackListSubscription();
  }
  return weak_tab_android_->RegisterModalUIChanged(std::move(callback));
}

bool TabInterfaceAndroid::IsInNormalWindow() const {
  if (!weak_tab_android_) {
    return false;
  }
  return weak_tab_android_->IsInNormalWindow();
}

tabs::TabFeatures* TabInterfaceAndroid::GetTabFeatures() {
  if (!weak_tab_android_) {
    return nullptr;
  }
  return weak_tab_android_->GetTabFeatures();
}

bool TabInterfaceAndroid::IsPinned() const {
  if (!weak_tab_android_) {
    return false;
  }
  return weak_tab_android_->IsPinned();
}

bool TabInterfaceAndroid::IsSplit() const {
  if (!weak_tab_android_) {
    return false;
  }
  return weak_tab_android_->IsSplit();
}

std::optional<tab_groups::TabGroupId> TabInterfaceAndroid::GetGroup() const {
  if (!weak_tab_android_) {
    return std::nullopt;
  }
  return weak_tab_android_->GetGroup();
}

std::optional<split_tabs::SplitTabId> TabInterfaceAndroid::GetSplit() const {
  if (!weak_tab_android_) {
    return std::nullopt;
  }
  return weak_tab_android_->GetSplit();
}

tabs::TabCollection* TabInterfaceAndroid::GetParentCollection(
    base::PassKey<tabs::TabCollection> pass_key) const {
  if (!weak_tab_android_) {
    return nullptr;
  }
  return weak_tab_android_->GetParentCollection(pass_key);
}

void TabInterfaceAndroid::OnReparented(
    tabs::TabCollection* parent,
    base::PassKey<tabs::TabCollection> pass_key) {
  if (!weak_tab_android_) {
    return;
  }
  return weak_tab_android_->OnReparented(parent, pass_key);
}

void TabInterfaceAndroid::OnAncestorChanged(
    base::PassKey<tabs::TabCollection> pass_key) {
  if (!weak_tab_android_) {
    return;
  }
  return weak_tab_android_->OnAncestorChanged(pass_key);
}
