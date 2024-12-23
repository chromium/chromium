// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_CORAL_ITEM_H_
#define ASH_BIRCH_BIRCH_CORAL_ITEM_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/coral_constants.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace ash {

class ASH_EXPORT BirchCoralItem : public BirchItem {
 public:
  // The actions that perform to a coral group. These values are persisted to
  // logs. Entries should not be renumbered and numeric values should never be
  // reused.
  enum class ActionType {
    kRestore,
    kLaunchToNewDesk,
    kSaveAsDeskTemplate,
    kMaxValue = kSaveAsDeskTemplate,
  };

  BirchCoralItem(const std::u16string& coral_title,
                 const std::u16string& coral_text,
                 CoralSource source,
                 const base::Token& group_id);
  BirchCoralItem(BirchCoralItem&&);
  BirchCoralItem(const BirchCoralItem&);
  BirchCoralItem& operator=(const BirchCoralItem&);
  bool operator==(const BirchCoralItem& rhs) const;
  ~BirchCoralItem() override;

  const base::Token& group_id() const { return group_id_; }

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction() override;
  void LoadIcon(LoadIconCallback callback) const override;
  BirchAddonType GetAddonType() const override;
  std::u16string GetAddonAccessibleName() const override;

  base::Value::Dict ToCoralItemDetails() const;

 private:
  // Helper method that calls `birch_client` to retrieve the image from
  // `favicon_service`, and passes the result back to `barrier_callback`.
  void GetFaviconImageCoral(
      const GURL& url,
      base::OnceCallback<void(const ui::ImageModel&)> barrier_callback) const;

  // Helper method that uses `saved_desk_delegate` to retrieve the app icon
  // image, and passes the result back to `barrier_callback`.
  void GetAppIconCoral(
      const std::string& app_id,
      base::OnceCallback<void(const ui::ImageModel&)> barrier_callback) const;

  CoralSource source_;
  base::Token group_id_;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_CORAL_ITEM_H_
