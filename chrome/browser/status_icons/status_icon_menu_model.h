// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_MENU_MODEL_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_MENU_MODEL_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/base/models/simple_menu_model.h"

namespace gfx {
class Image;
}

// StatusIconMenuModel contains the state of the SimpleMenuModel as well as that
// of its delegate. This is done so that we can easily identify when the menu
// model state has changed and can tell the status icon to update the menu. This
// is necessary some platforms which do not notify us before showing the menu
// (like Ubuntu Unity).
class StatusIconMenuModel
    : public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate,
      public base::SupportsWeakPtr<StatusIconMenuModel> {
 public:
  class Delegate {
   public:
    // Performs the action associates with the specified command id.
    // The passed |event_flags| are the flags from the event which issued this
    // command and they can be examined to find modifier keys.
    virtual void ExecuteCommand(int command_id, int event_flags) = 0;

   protected:
    virtual ~Delegate() {}
  };

  class Observer {
   public:
    // Invoked when the menu model has changed.
    virtual void OnMenuStateChanged() {}

   protected:
    virtual ~Observer() {}
  };

  // The Delegate can be NULL.
  explicit StatusIconMenuModel(Delegate* delegate);
  ~StatusIconMenuModel() override;

  // Methods for seting the state of specific command ids.
  void SetCommandIdChecked(int command_id, bool checked);
  void SetCommandIdEnabled(int command_id, bool enabled);
  void SetCommandIdVisible(int command_id, bool visible);

  // Sets the accelerator for the specified command id.
  void SetAcceleratorForCommandId(
      int command_id, const ui::Accelerator* accelerator);

  // Calling any of these "change" methods will mark the menu item as "dynamic"
  // (see menu_model.h:IsItemDynamicAt) which many platforms take as a cue to
  // refresh the label and icon of the menu item each time the menu is
  // shown.
  void ChangeLabelForCommandId(int command_id, const base::string16& label);
  void ChangeIconForCommandId(int command_id, const gfx::Image& icon);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool GetIconForCommandId(int command_id, gfx::Image* icon) const override;

 protected:
  // Overriden from ui::SimpleMenuModel:
  void MenuItemsChanged() override;

  void NotifyMenuStateChanged();

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }
  Delegate* delegate() { return delegate_; }

 private:
  // Overridden from ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  struct ItemState;

  // Map the properties to the command id (used as key).
  typedef std::map<int, ItemState> ItemStateMap;

  ItemStateMap item_states_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconMenuModel);
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_MENU_MODEL_H_
