// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DESK_TEMPLATE_H_
#define ASH_PUBLIC_CPP_DESK_TEMPLATE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "components/full_restore/restore_data.h"

namespace ash {

// Class to represent a desk template. It can be used to create a desk with
// a certain set of application windows specified in |desk_restore_data_|.
class ASH_PUBLIC_EXPORT DeskTemplate {
 public:
  DeskTemplate();
  explicit DeskTemplate(double uuid);
  DeskTemplate(const DeskTemplate&) = delete;
  DeskTemplate& operator=(const DeskTemplate&) = delete;
  ~DeskTemplate();

  double uuid() const { return uuid_; }
  const std::u16string& template_name() const { return template_name_; }
  void set_template_name(const std::u16string& template_name) {
    template_name_ = template_name;
  }
  full_restore::RestoreData* desk_restore_data() {
    return desk_restore_data_.get();
  }
  void set_desk_restore_data(
      std::unique_ptr<full_restore::RestoreData> restore_data) {
    desk_restore_data_ = std::move(restore_data);
  }

 private:
  const double uuid_;  // We'll use the current time in seconds since epoch to
                       // uniquely identify the template.
  std::u16string template_name_;

  // Contains the app launching and window information that can be used to
  // create a new desk instance with the same set of apps/windows specified in
  // it.
  std::unique_ptr<full_restore::RestoreData> desk_restore_data_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DESK_TEMPLATE_H_
