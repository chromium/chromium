// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DESK_TEMPLATE_H_
#define ASH_PUBLIC_CPP_DESK_TEMPLATE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/guid.h"
#include "base/time/time.h"
#include "components/app_restore/restore_data.h"

namespace ash {

// Class to represent a desk template. It can be used to create a desk with
// a certain set of application windows specified in |desk_restore_data_|.
class ASH_PUBLIC_EXPORT DeskTemplate {
 public:
  DeskTemplate();
  explicit DeskTemplate(const base::GUID& uuid);
  // This constructor is used in the instantiation of the DeskTemplate from
  // a WorkspaceDeskSpecifics proto and base::Value.
  DeskTemplate(const std::string& uuid,
               const std::string& name,
               const base::Time& time_created);
  DeskTemplate(const DeskTemplate&) = delete;
  DeskTemplate& operator=(const DeskTemplate&) = delete;
  ~DeskTemplate();

  // Used in cases where copies of a DeskTemplate are needed to be made.
  // This specifically used in the DeskSyncBridge which requires a map
  // of DeskTemplate unique pointers to be valid and needs to pass
  // that information in DeskModel callbacks.
  std::unique_ptr<DeskTemplate> Clone();

  base::GUID uuid() const { return uuid_; }
  base::Time created_time() const { return created_time_; }
  const std::u16string& template_name() const { return template_name_; }
  void set_template_name(const std::u16string& template_name) {
    template_name_ = template_name;
  }

  const app_restore::RestoreData* desk_restore_data() const {
    return desk_restore_data_.get();
  }

  void set_desk_restore_data(
      std::unique_ptr<app_restore::RestoreData> restore_data) {
    desk_restore_data_ = std::move(restore_data);
  }

 private:
  const base::GUID uuid_;  // We utilize the string based base::GUID to uniquely
                           // identify the template.

  const base::Time created_time_;  // We'll use the current time in seconds
                                   // since the Windows epoch.
  std::u16string template_name_;

  // Contains the app launching and window information that can be used to
  // create a new desk instance with the same set of apps/windows specified in
  // it.
  std::unique_ptr<app_restore::RestoreData> desk_restore_data_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DESK_TEMPLATE_H_
