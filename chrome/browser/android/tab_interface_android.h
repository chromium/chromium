// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_INTERFACE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_INTERFACE_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

class TabAndroid;

// Wraps a WeakPtr to a `TabAndroid` in a class compatible with being in a
// std::unique_ptr. This allows compatibility with tab collections without
// changing the memory model of Android tabs. Tab lifecycle on Android is
// managed via a ref counted Java object that explicitly destroys the C++
// component as part of an explicit destroy method.
class TabInterfaceAndroid : public tabs::TabInterface {
 public:
  explicit TabInterfaceAndroid(TabAndroid* tab_android);
  ~TabInterfaceAndroid() override;

  TabInterfaceAndroid(const TabInterfaceAndroid&) = delete;
  void operator=(const TabInterfaceAndroid&) = delete;

  // TabInterface overrides:
  base::WeakPtr<tabs::TabInterface> GetWeakPtr() override;
  content::WebContents* GetContents() const override;
  void Close() override;
  base::CallbackListSubscription RegisterWillDiscardContents(
      WillDiscardContentsCallback callback) override;
  bool IsActivated() const override;
  base::CallbackListSubscription RegisterDidActivate(
      DidActivateCallback callback) override;
  base::CallbackListSubscription RegisterWillDeactivate(
      WillDeactivateCallback callback) override;
  bool IsVisible() const override;
  bool IsSelected() const override;
  base::CallbackListSubscription RegisterDidBecomeVisible(
      DidBecomeVisibleCallback callback) override;
  base::CallbackListSubscription RegisterWillBecomeHidden(
      WillBecomeHiddenCallback callback) override;
  base::CallbackListSubscription RegisterWillDetach(
      WillDetach callback) override;
  base::CallbackListSubscription RegisterDidInsert(
      DidInsertCallback callback) override;
  base::CallbackListSubscription RegisterPinnedStateChanged(
      PinnedStateChangedCallback callback) override;
  base::CallbackListSubscription RegisterGroupChanged(
      GroupChangedCallback callback) override;
  bool CanShowModalUI() const override;
  std::unique_ptr<tabs::ScopedTabModalUI> ShowModalUI() override;
  base::CallbackListSubscription RegisterModalUIChanged(
      TabInterfaceCallback callback) override;
  bool IsInNormalWindow() const override;
  tabs::TabFeatures* GetTabFeatures() override;
  const tabs::TabFeatures* GetTabFeatures() const override;
  bool IsPinned() const override;
  bool IsSplit() const override;
  std::optional<tab_groups::TabGroupId> GetGroup() const override;
  std::optional<split_tabs::SplitTabId> GetSplit() const override;
  tabs::TabCollection* GetParentCollection(
      base::PassKey<tabs::TabCollection>) const override;
  const tabs::TabCollection* GetParentCollection() const override;
  void OnReparented(tabs::TabCollection* parent,
                    base::PassKey<tabs::TabCollection>) override;
  void OnAncestorChanged(base::PassKey<tabs::TabCollection>) override;
  ui::UnownedUserDataHost& GetUnownedUserDataHost() override;
  const ui::UnownedUserDataHost& GetUnownedUserDataHost() const override;

 private:
  ui::UnownedUserDataHost unowned_user_data_host_;
  base::WeakPtr<TabAndroid> weak_tab_android_;
};

#endif  // CHROME_BROWSER_ANDROID_TAB_INTERFACE_ANDROID_H_
